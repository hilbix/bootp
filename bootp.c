/* Shell scripted bootstrapping service on an interface for zero config installs of VMs
 *
 * Implemented: 	BOOTP	https://datatracker.ietf.org/doc/html/rfc951
 * Partly implemented:	DHCPv4	https://datatracker.ietf.org/doc/html/rfc2131	https://datatracker.ietf.org/doc/html/rfc2132
 * Not implemented:	DHCPv6	https://datatracker.ietf.org/doc/html/rfc3315	(not needed for bootstrapping VMs)
 *
 * Implemented does not mean this fully conforms to or is fully compatible to the RFC.
 * This only means there is nothing left in the RFC which needs to be implemented here.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include <netdb.h>
#include <arpa/inet.h>

static int debug;
#define	DEBUG_RECV	1
#define	DEBUG_SEND	2
#define	DEBUG_BOOTP	4
#define	DEBUG_DHCP	8
#define	DEBUG_CHAT	16
#define	DEBUG_MISC	32
#define	DEBUG(X)	(debug&DEBUG_##X)

#define	FATAL(test)	do { if (test) OOPS(#test); } while (0)

void _Noreturn
OOPS(const char *s)
{
  perror(s);
  exit(23);
}

void
err(const char *s)
{
  perror(s);
}

/* sigh, quiesce compiler warning: ‘strncpy’ specified bound XX equals destination size [-Wstringop-truncation] */
char *
my_strncpy(char *dest, const char *src, size_t len)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
  return strncpy(dest, src, len);
#pragma GCC diagnostic pop
}

struct addrinfo *
get_addr(const char *host, const char *port)
{
  static struct addrinfo	*ret;
  struct addrinfo		hint;

  /* As we are single threaded,
   * we cache the last answer,
   * so we can free it at the next call.
   */
  if (ret)
    freeaddrinfo(ret);
  ret	= 0;

  memset(&hint, 0, sizeof hint);
  hint.ai_family	= AF_INET;
  hint.ai_socktype	= SOCK_DGRAM;
  hint.ai_flags		= AI_IDN|AI_PASSIVE;

  return getaddrinfo(host, "67", &hint, &ret) < 0 ? 0 : ret;
}

struct sockaddr_in *
get_addr4(const char *host, const char *port)
{
  struct addrinfo *ai;

  ai	= get_addr(host, port);
  for (;; ai = ai->ai_next)
    {
      if (!ai)
        {
          printf("cannot resolve %s %s\n", host, port);
          return 0;
        }
      if (ai->ai_family == AF_INET)
        return (struct sockaddr_in *)ai->ai_addr;
    }
 }

int
udp_bound(const char *interface, const char *host, const char *port)
{
  struct addrinfo	*ai;
  int			fd;

  ai	= get_addr(host, port);
  if (!ai)
    OOPS("cannot resolve");

  for (;; ai = ai->ai_next)
    {
      if (!ai)
        OOPS("cannot bind socket");

      if ((fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) < 0)
        OOPS("cannot create socket");

      if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, interface, strlen(interface)) < 0)
        OOPS("cannot use interface");

      if (bind(fd, ai->ai_addr, ai->ai_addrlen) == 0)
        break;

      close(fd);
    }
  return fd;
}

int
udp_bc(const char *interface, int port)
{
  struct sockaddr_in	sa4;
  int			fd, bc;

  if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    OOPS("cannot create UDP socket");

  bc	= 1;
  if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &bc, sizeof bc) < 0)
    OOPS("broadcast permission failed");

  if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, interface, strlen(interface)) < 0)
    OOPS("cannot use interface");

  sa4.sin_family	= AF_INET;
  sa4.sin_port		= htons(port);
  sa4.sin_addr.s_addr	= htonl(INADDR_ANY);

  if (bind(fd, &sa4, sizeof sa4) < 0)
    OOPS("cannot bind to broadcast address");

  return fd;
}

#define	BOOTREQUEST	1
#define	BOOTREPLY	2

#define	BOOTP_MINSIZE	4+4+2+2+4*4+16+64+128+64	/* 300	*/
#define	MAX_PKG_LEN	2000

const char DHCP_MAGIC[]	= { 0x63, 0x82, 0x53, 0x63 };

struct bootp	/* see http://www.tcpipguide.com/free/t_BOOTPMessageFormat.htm	*/
  {
    unsigned char	op;		/* BOOTREQUEST or BOOTREPLY	*/
    unsigned char	htype;		/* 1:Ethernet	*/
    unsigned char	hlen;		/* 6:Ethernet	*/
    unsigned char	hops;		/* 0:number of hops */
    u_int32_t		xid;		/* 32 bit transaction ID */
    u_int16_t		secs;		/* ignored: seconds since boot began */
    u_int16_t		flags;		/* RFC1542 flags: Bit 0 = use broadcast reply */
    u_int32_t		ciaddr;		/* NBO: wanted IP	*/
    u_int32_t		yiaddr;		/* NBO: assigned IP	*/
    u_int32_t		siaddr;		/* NBO: TFTP server	*/
    u_int32_t		giaddr;		/* NBO: relay (0)	*/
    unsigned char	chaddr[16];	/* MAC	*/
    char		sname[64];	/* opt: wanted/replying server name */
    char		file[128];	/* PXE filename	*/
    unsigned char	vend[64];	/* ignored: vendor-specific area */
  };

struct decoded
  {
    unsigned char	*data;
    int			size;
    uint16_t		pos[256];
    uint8_t		len[256];
    uint8_t		bytes[256][4];
  };

void
print_addr(struct sockaddr_in *sa, int len, const char *what, ...)
{
  const char	*s;
  char		buf[128];
  va_list	list;

#if 1
  FATAL(sa->sin_family != AF_INET);
  /* WTF why do I have to pass a newly founded wheel here?	*/
  s	= inet_ntop(sa->sin_family, &((struct sockaddr_in *)sa)->sin_addr, buf, sizeof buf);
#else
  FATAL(sa->sa_family != AF_INET6);
  s	= inet_ntop(sa->sa_family, &((struct sockaddr_in6 *)sa)->sin6_addr, buf, sizeof buf);
#endif

  FATAL(!s);

  va_start(list, what);
  vprintf(what, list);
  va_end(list);
  printf(" len %d addr %s\n", len, s);
}

static void
xd_(const char *what, void *ptr, int len, int w, int pad)
{
  int	i, pos, max, end;

  for (end=len; --end>=0; )
    if (((unsigned char *)ptr)[end])
      break;
  if ((end+=1) > len-4)
    end	= len;

  for (pos=0; pos<len; pos+=w)
    {
      max	= len-pos;
      if (max>w) max = w;

      printf("%s %03x", what, pos);
      for (i=0; i<max; i++)
        if (pos+i >= end && i < w-2)
          {
            printf(" %02x *%4d", ((unsigned char *)ptr)[pos+i], len-end);
            if (!i)
              {
                printf("\n");
                return;
              }
            len	= pos;	/* terminate early	*/
            i	+= 3;
            break;
          }
        else
          printf(" %02x", ((unsigned char *)ptr)[pos+i]);
      while (++i<=w)
        printf("   ");
      printf("%*s ! ", pad, "");
      for (i=0; i<max; i++)
        {
          unsigned char c = ((unsigned char *)ptr)[pos+i];
          printf("%c", isprint(c) ? c : '.');
        }
      printf(" ");
      while (++i<=w)
        printf("_");
      printf("!\n");
    }
}

static void
xd(const char *what, void *ptr, int len)
{
  xd_(what, ptr, len, 32, 3);
}

#define	XD(what,elem)	xd(what,&(elem),sizeof (elem))

#include "dhcp.h"

void
decode_dhcp(struct decoded *decode, int debugging)
{
  unsigned char	c;
  int		i;
  unsigned char	*buf	= decode->data;
  int		len	= decode->size;

  c	= 0;
  for (i=0; i<len; )
    {
      char		tmp[40];
      const char	*t;
      int		n, p;
      const char *(*fn)(const unsigned char *);

      n	= 5;
      switch (c = buf[i])
        {
        case 0xff:
          p	= 1;
          t	= "END";
          n	= len-i;
          break;
        default:
          p	= 2;
          t	= "?";
          n	= 2 + buf[i+1];
          break;
        }
      fn	= 0;
      if (c < sizeof DHCPoptions / sizeof *DHCPoptions)
        {
          struct DHCPoptions *d = DHCPoptions+c;

          t	= d->name;
          if (d->n < 0)		/* NOP	*/
            {
              n	= -d->n;
              p	= 1;
            }
          else if (!d->n || d->n+2 == n)
            fn	= d->fn;
          else if (debugging)
            printf("DHCP fixed length mismatch, should be %d\n", d->n);
        }
      decode->len[c]	= n-p;
      decode->pos[c]	= i+p;
      memcpy(&decode->bytes[c], buf+i+p, n-p > 4 ? 4 : n-p);
      if (debugging)
        {
          snprintf(tmp, sizeof tmp, "DHCP %03x %12s %3d %3d+%d", i, t, c, n-p, p);
          if (fn && (t = fn(buf+i))!=0)
            printf("%s %s\n", tmp, t);
          else
            xd_(tmp, buf+i, i+n>len ? len-i : n, 24, 0);
        }
      i	+= n;
    }
  if (!debugging)
    return;
  if (i>len)
    printf("DHCP %4x option truncated\n", len);
  else if (c!=0xff)
    printf("DHCP %4x missing END marker\n", len);
}

void
print_pkt(void *buf, int len, struct decoded *decode, int debugging)
{
  struct bootp *b = (void *)buf;

  if (debugging && DEBUG(BOOTP))
    {
      xd("HEAD", &b->op, 4);
      XD("ID  ", b->xid);
      XD("SEC ", b->secs);
      XD("FLAG", b->flags);
      XD("WANT", b->ciaddr);
      XD("ADR ", b->yiaddr);
      XD("TFTP", b->siaddr);
      XD("GATE", b->giaddr);
      XD("MAC ", b->chaddr);
      XD("NAME", b->sname);
      XD("FILE", b->file);
      xd("VEND", b->vend, len - offsetof(struct bootp, vend));
    }
  if (decode && len > offsetof(struct bootp, vend)+sizeof DHCP_MAGIC)
    {
      if (!memcmp(b->vend, DHCP_MAGIC, sizeof DHCP_MAGIC))
        {
          decode->data	= b->vend + sizeof DHCP_MAGIC;
          decode->size	= len - offsetof(struct bootp, vend) - sizeof DHCP_MAGIC;
          decode_dhcp(decode, debugging && DEBUG(DHCP));
        }
      else if (debugging && DEBUG(DHCP))
        printf("DHCP magic missing: %02x%02x%02x%02x\n", b->vend[0], b->vend[1], b->vend[2], b->vend[3]);
    }
}

union sa
  {
    struct sockaddr	sa;
    struct sockaddr_in	sa4;
  };

int
reply(const char *script, void *buf, size_t len, struct sockaddr_in *sa, const char *override)
{
  struct bootp *b = buf;
  /* buf contains the already modified buffer
   * override contains a possible changed destination: [port [IP]]
   */

  unsigned long port = 68;
  u_int32_t	ip = ntohs(b->flags & 0x8000) ? htons(INADDR_BROADCAST) : b->yiaddr;

  sa->sin_family	= AF_INET;
  sa->sin_addr.s_addr	= ip;
  sa->sin_port		= htons(port);
  if (!*override)
    return 0;

  char	*end;

  end	= 0;
  port	= strtoul(override, &end, 0);
  if (end && *end==' ')
    {
      struct sockaddr_in *in;

      *end++ = 0;
      in	= get_addr4(end, port ? override : "68");
      if (!in)
        return -1;
      sa->sin_addr	= in->sin_addr;
      sa->sin_port	= in->sin_port;
      return 0;
    }

  printf("unusable reply from script %s: %s\n", script, override);
  return -1;
}

const char *
ip(int nr, void *a)
{
  unsigned char *p = a;
  static char ips[6][16];

  snprintf(ips[nr], 16, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
  return ips[nr];
}

const char *
bip(int nr, struct bootp *b)
{
  return ip(nr, ((&b->ciaddr)+nr));
}

const char *
bmac(struct bootp *b)
{
  static char buf[18];
  unsigned char *c = b->chaddr;

  snprintf(buf, sizeof buf, "%02x:%02x:%02x:%02x:%02x:%02x", c[0], c[1], c[2], c[3], c[4], c[5]);
  return buf;
}

const char *
bxid(struct bootp *b)
{
  static char	xid[9];

  snprintf(xid, sizeof xid, "%08x", ntohl(b->xid));
  return xid;
}

const char *
bsname(struct bootp *b)
{
  static char	sname[1+sizeof b->sname];

  memcpy(sname, b->sname, sizeof b->sname);
  sname[sizeof b->sname] = 0;
  return sname;
}

const char *
bfile(struct bootp *b)
{
  static char	file[1+sizeof b->file];

  memcpy(file, b->file, sizeof b->file);
  file[sizeof b->file] = 0;
  return file;
}

char *
hostname(void)
{
  static char	*hostname;

  if (hostname) return hostname;

  struct utsname un;

  uname(&un);
  hostname	= strdup(un.nodename);
  FATAL(!hostname);
  return hostname;
}

uint32_t
fromip(const char *ip)
{
  struct sockaddr_in	*in;

  in	= get_addr4(ip, "1");
  return in->sin_addr.s_addr;
}

uint16_t
from16(const char *number)
{
  return htons(strtoul(number, NULL, 0));
}

uint32_t
from32(const char *number)
{
  return htons(strtoul(number, NULL, 0));
}

int
set_vend(struct bootp *b, const char *s)
{
  const char	*pos = s;
  const	int	off = offsetof(struct bootp, vend);
  int		i, max;

  max	= MAX_PKG_LEN-off;

  for (i=0; *pos; i++)
    {
      int v, end;

      if (i >= max)
        {
          printf("overflow at %d: %s\n", i, pos);
          return -1;
        }
      if (sscanf(pos, " %x %n", &v, &end)!=1)
        {
          printf("sscanf did not work at %d: %s\n", i, pos);
          return -1;
        }
      if (v<0 || v>255)
        {
          printf("value %d out of range at %d: %s\n", v, i, pos);
          return -1;
        }
      b->vend[i]	= v;
      pos		+= end;
    }
  max	= (sizeof *b) - off;
  while (i<max)
    b->vend[i++] = 0;
  return i+off;
}

int
putN(unsigned char *buf, unsigned long u, int n)
{
  while (--n>=0)
    {
      buf[n]	= u;
      u	>>= 8;
    }
  return u!=0;
}

const char *
getIP1(const char *p, unsigned char *buf)
{
  unsigned long	u;
  char		*end;

  u = strtoul(p, &end, 10);
  if (!end || end == p || u>255)
    return 0;
  *buf	= u;
  return end;
}

int
putIP(unsigned char *buf, const char **s)
{
  const	char *p	= *s;

  p = getIP1(p, buf);
  if (!p || *p++!='.') return 1;
  p = getIP1(p, buf+1);
  if (!p || *p++!='.') return 1;
  p = getIP1(p, buf+2);
  if (!p || *p++!='.') return 1;
  p = getIP1(p, buf+3);
  if (!p) return 1;

  *s	= p;
  return 0;
}

int
unhex(char c)
{
  if (c>='0' && c<='9') return c-'0';
  if (c>='A' && c<='F') return c-'A'+10;
  if (c>='a' && c<='f') return c-'a'+10;
  return -1;
}

int
putSTR(unsigned char *buf, const char **s, int max, int hex)
{
  const	char	*p	= *s;
  int		n;
  
  for (n=0;; p++, n++)
    {
      int	c;

      if (hex)
        while (*p == ' ') p++;
      if (!(c = (unsigned char)*p))
        break;
      if (hex)
        {
          if (!p[1])
            {
              printf("uneven number of hex digits!\n");
              return -1;
            }
          c	= (unhex(c) << 4) | unhex(*++p);
          if (c<0)
            {
              printf("invalid hex digits!\n");
              return -1;
            }
        }
      *buf++ = c;
    }
  *s	= p;	/* *s == 0	*/
  return n;
}

/* DHCP NR type data
 *
 * NR is decimal, not HEX, see dhcp.md
 *
 * types:
 * str	copy data as string
 * hex	hex bytes with or without spaces
 * ip	IPv4 in network order
 * mask	IPv4/MASK pairs
 * 1	decimal 8 bit value
 * 2	decimal 16 bit value
 * 4	decimal 32 bit value
 */
int
set_dhcp(unsigned char *buf, int off, const char *s)
{
  int		max = MAX_PKG_LEN - offsetof(struct bootp, vend);
  unsigned long	nr;
  int		i, l;
  char		t;

  /* check for buf being already at end here?	*/
  for (;;)
    {
      char		*end;

      nr	= strtoul(s, &end, 0);
      if (!end || end == s || nr>255)
        {
          printf("invalid DHCP at %d: %s\n", off, s);
          return -1;
        }

      if (off>=max)
        {
          printf("overflow DHCP %lu at %d: %s\n", nr, off, s);
          return -1;
        }

      s			= end;
      buf[off++]	= nr;
      while (*s == ' ') s++;

      if (nr==255)
        {
          if (*s)
            {
              printf("END option cannot have additional data at %d: %s\n", off, s);
              return -1;
            }
          max	= sizeof ((struct bootp *)0)->vend;
          while (off<max)
            buf[off++] = 0;
          return off;
        }
      if (nr)
        break;
      if (!*s)
        return off;
    }
  /* 0 and 255 are handled above
   * We now need at least 2 bytes
   */
  t	= *s;
  while (*s && *s!=' ') s++;
  while (*s == ' ') s++;
  l	= off++;
  i	= 0;
  do
    {
      int	k;

      if (!*s || !t)
        {
          printf("premature end of line for DHCP %lu at %d after type %c\n", nr, off, t);
          return -1;
        }
      switch (t)
        {
        default:
          printf("unknown type %c of line for DHCP %lu at %d\n", t, nr, off);
          return -1;
        case '1': k=1; break;
        case '2': k=2; break;
        case '4': k=4; break;
        case 'm': k=8; break;
        case 'i': k=4; break;
        case 's':
          k	= putSTR(buf+off, &s, max-off, 0);
          break;
        case 'h':
          k	= putSTR(buf+off, &s, max-off, 16);
          break;
        }
      if (k<0 || off+k>=max)
        {
          printf("overflow DHCP %lu at %d: %s\n", nr, off, s);
          return -1;
        }
      i	+= k;
      switch (t)
        {
        unsigned long	v;
        char		*end;

        case '1':
        case '2':
        case '4':
          v	= strtoul(s, &end, 0);
          if (!end || end == s)
            {
              printf("DHCP %lu type %c needs number at %d: %s\n", nr, t, off, s);
              return -1;
            }
          if (putN(buf+off, v, k))
            {
              printf("DHCP %lu type %c overflow at %d: %s\n", nr, t, off, s);
              return -1;
            }
          s	= end;
          break;
        case 'm':
          if (putIP(buf+off, &s) || *s != '/')
            {
              printf("DHCP %lu mask needs ip/mask at %d: %s\n", nr, off, s);
              return -1;
            }
          s++;
        case 'i':
          if (putIP(buf+off, &s) || (*s && *s!=' '))
            {
              printf("DHCP %lu needs ip at %d: %s\n", nr, off, s);
              return -1;
            }
          break;
        }
      off	+= k;
      while (*s == ' ') s++;
    } while (*s);
  FATAL(l != off - i - 1);
  buf[l]	= i;
  if (nr < sizeof DHCPoptions / sizeof *DHCPoptions)
    {
      struct DHCPoptions *o = DHCPoptions+nr;

      if (o->n>0 && i != o->n)
        {
          printf("wrong length for type %c for DHCP %lu at %d\n", t, nr, off);
          return -1;
        }
    }
  return off;
}

void
PUT_env(const char *s, ...)
{
  va_list	list;
  char		tmp[200];

  va_start(list, s);
  vsnprintf(tmp, sizeof tmp, s, list);
  va_end(list);
#if 0
  fprintf(stderr, "%s\n", tmp); fflush(stderr);
#endif
  putenv(strdup(tmp));
}


/* fork a script with parameters.
 * process output to adapt buffer
 */
char *
request(const char *script, void *buf, int *len, struct decoded *decode, struct sockaddr_in *sa, const char *interface)
{
  struct bootp	*b = buf;
  int		fds[2];
  pid_t		pid;
  int		dhcp	= 0;
  FILE		*fd;

#if 0
  /* let script decide	*/
  /* following logic was stolen from bootp:	*/
  const char *me	= hostname();
  if (*b->sname)
    {
      struct hostent *h = gethostbyname(b->sname);
      if (!h)
        {
          err("cannot resolve server name in request");
          return 0;
        }
      if (strcmp(h->h_name, me))
        return 0;	/* no need to free()?	*/
    }
#endif

  if (pipe(fds))
    {
      err("pipe()");
      return 0;
    }
  if ((pid = fork()) == 0)
    {
      close(fds[0]);
      if (fds[1] != 1)
        {
          dup2(fds[1], 1);
          close(fds[1]);
        }

      /* feed buffer to script as a child	*/
      if (pipe(fds))
        {
          err("pipe()");
          exit(-1);
        }
      if ((pid = fork()) == 0)
        {
          int	ret;

          close(fds[0]);
          fd	= fdopen(fds[1], "w");
          if (!fd)
            {
              err("fdopen");
              exit(-1);
            }
          /* ignore errors, pipe closed is too often	*/
          fwrite(buf, *len, 1, fd);
          fflush(fd);
          ret = ferror(fd);
          fclose(fd);
          exit(ret ? -1 : 0);
        }
      close(fds[1]);
      if (fds[0] != 0)
        {
          dup2(fds[0], 0);
          close(fds[0]);
        }

      int i, x;

      /* build environment from decode	*/
      for (i=0; i<256; i++)
        if ((x = decode->pos[i])!=0)
          {
            static char bytes[] = "DHCP%d_bytes=%02x %02x %02x %02x";

            PUT_env("DHCP%d_pos=%d", i, x);
            x	= decode->len[i];
            PUT_env("DHCP%d_len=%d", i, x);
            if (x<=0) continue;
            if (x>4)
              x	= 4;
            x	= 12 + 5*x;
            bytes[x] = 0;
            PUT_env(bytes, i, decode->bytes[i][0], decode->bytes[i][1], decode->bytes[i][2], decode->bytes[i][3]);
            bytes[x] = ' ';
            bytes[12 + 5*4] = 0;
          }
      const char *mac = bmac(b); /* faked for now --vvv */
      execlp(script, script, hostname(), interface, mac, ip(4, &sa->sin_addr), mac, bxid(b), bsname(b), bfile(b), bip(0, b), bip(1, b), bip(2, b), bip(3, b), NULL);
      err(script);
      exit(-1);
    }
  close(fds[1]);

  fd	= fdopen(fds[0], "r");
  if (!fd)
    {
      err("fdopen");
      return 0;
    }

  char	data[BUFSIZ];

  /* as we are single threaded and never fork() multiply,
   * we can cache the returned value and free it on the next call
   * Keep it simple.
   */
  static char *ret;

  if (ret)
    free(ret);
  ret	= strdup("");		/* used default reply port	*/

  /* we read lines of the form
   * KEY VALUE
   */
  for (;;)
    {
      char *line = fgets(data, sizeof data, fd);

      if (!line)
        {
          if (!ret)
            err("out of memory");
          break;
        }

      int cnt	= strlen(line);

      while (--cnt>=0 && isspace(line[cnt]));
      line[++cnt] = 0;
      if (!cnt) continue;

      if (DEBUG(CHAT)) printf("CHAT: %s\n", line);
      if (!strncmp("REPL ", line, 5))
        {
          /* REPL port IP
           * REPL 0 IP
           */
          if (ret) free(ret);
          ret	= strdup(line+5);
          continue;
        }
      if (!strncmp("SECS ", line, 5))
        {
          b->secs = from16(line+5);
          continue;
        }
      if (!strncmp("FLAG ", line, 5))
        {
          b->flags = from16(line+5);
          continue;
        }
      if (!strncmp("WANT ", line, 5))
        {
          b->ciaddr = fromip(line+5);
          continue;
        }
      if (!strncmp("ADDR ", line, 5))
        {
          b->yiaddr = fromip(line+5);
          continue;
        }
      if (!strncmp("TFTP ", line, 5))
        {
          b->siaddr = fromip(line+5);
          continue;
        }
      if (!strncmp("GATE ", line, 5))
        {
          b->giaddr = fromip(line+5);
          continue;
        }
#if 0
    unsigned char	chaddr[16];	/* MAC	*/
#endif
      if (!strncmp("HOST ", line, 5))
        {
          my_strncpy(b->sname, line+5, sizeof b->sname);
          continue;
        }
      if (!strncmp("FILE ", line, 5))
        {
          my_strncpy(b->file, line+5, sizeof b->file);
          continue;
        }
      if (!strncmp("VEND ", line, 5))
        {
          int tmp;

          tmp	 = set_vend(b, line+5);
          if (tmp>0)
            {
              FATAL(tmp < BOOTP_MINSIZE);
              dhcp	= 0;
              *len	= tmp;
              continue;
            }
        }
      if (!strncmp("DHCP ", line, 5))
        {
          int	tmp;

          if (!dhcp)
            {
              dhcp = sizeof DHCP_MAGIC;
              memcpy(b->vend, DHCP_MAGIC, sizeof DHCP_MAGIC);
            }
          tmp	 = set_dhcp(b->vend, dhcp, line+5);
          if (tmp>0)
            {
              dhcp	= tmp;
              *len	= tmp + offsetof(struct bootp, vend);
              continue;
            }
        }
      printf("terminating script %s due to wrong line: %s\n", script, line);
      if (ret) free(ret);
      ret = 0;
      kill(pid, SIGTERM);
      break;
    }
  fclose(fd);

  if (!ret)
    return 0;

  if (!*b->sname)
    my_strncpy(b->sname, hostname(), sizeof b->sname);

  int flag = WNOHANG;
  for (;;)
    {
      pid_t	id;
      int	status;

      id	= waitpid(-1, &status, flag);
      if (id == (pid_t)-1)
        {
          if (errno == EINTR) continue;
          OOPS("waitpid errored");
        }
      if (id == pid)
        {
          if (WIFEXITED(status))
            {
              int code	= WEXITSTATUS(status);
              if (!code)
                break;
              printf("script %s failed with code %d\n", script, code);
            }
          else if (WIFSIGNALED(status))
            printf("script %s terminated with signal %d\n", script, (int)WTERMSIG(status));
          else
            printf("script %s termination status unsupported (%x)\n", script, status);
          return 0;
        }
      if (!id)
        {
          /* Do not try to be clever.
           * We want the main script to return status.
           * (We cannot check for early termination,
           * because it can take time to read the pipe.)
           */
          if (DEBUG(MISC))
            printf("Script %s should not close stdout before terminating.\n", script);
          flag	= 0;
        }
    }
  return b->yiaddr ? ret : 0;
}

int
main(int argc, char **argv)
{
  const char	*interface;
  int		fd;
  int		cleanup;

  FATAL(BOOTP_MINSIZE != sizeof(struct bootp));
  FATAL(DHCP_LAST_KNOWN_OPTION+1 != sizeof DHCPoptions/sizeof *DHCPoptions);

  if (argc!=2)
    {
      fprintf(stderr, "Usage: %s interface\n", argv[0]);
      return 42;
    }

  if (getenv("DEBUG"))
    {
      debug	= atoi(getenv("DEBUG"));
      printf("debugging set to 0x%x\n", debug);
    }

  interface	= argv[1];
  fd		= udp_bc(interface, 67);

  cleanup	= 0;
  for (;;)
    {
      static char	buf[MAX_PKG_LEN];
      static struct decoded decode;
      int		got;
      union sa		sa;
      unsigned		salen;
      int		flag;

      flag	= fcntl(fd, F_GETFL, 0);
      if (flag < 0)
        OOPS("fcntl(F_GETFL)");
      if (!(flag & O_NONBLOCK) != !cleanup)
        {
          flag	= cleanup ? (flag|O_NONBLOCK) : (flag&~O_NONBLOCK);
          if (fcntl(fd, F_SETFL, flag))
            OOPS("fcntl(F_SETFL)");
        }

      salen	= sizeof sa;
      got = recvfrom(fd, buf, sizeof buf, 0, &sa.sa, &salen);
      if (got < 0)
        {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
              if (--cleanup)
                printf("%d packet(s) ignored\n", cleanup);
              cleanup	= 0;
            }
          else
            err("recv");
          continue;
        }
      if (got > sizeof buf)
        OOPS("packet buffer overrun");

      if (sa.sa.sa_family != AF_INET)
        {
          printf("socket family %d: %x not AF_INET %x\n", got, (int)sa.sa.sa_family, (int)AF_INET);
          xd("ADR", &sa, salen);
          if (DEBUG(MISC))
            xd("PKT", buf, got);
          continue;
        }
      if (got < offsetof(struct bootp, vend) + 5)
        {
          print_addr(&sa.sa4, got, "too short BOOTP (min %d)", (int)(offsetof(struct bootp, vend) + 5));
          if (DEBUG(MISC))
            xd("PKT", buf, got);
          continue;
        }
      if (buf[1] != 1 || buf[2] != 6)
        {
          print_addr(&sa.sa4, got, "packet frame %2x %2x should be 01 06", buf[0], buf[1]);
          if (DEBUG(MISC))
            xd("PKT", buf, got);
          continue;
        }

      const char *script;

      switch (buf[0])
        {
        default:
          print_addr(&sa.sa4, got, "unknown BOOTP %02x", (unsigned char)buf[0]);
          continue;
        case BOOTREPLY:
          print_addr(&sa.sa4, got, "BOOTREPLY %s", bxid((struct bootp *)buf));
          script="./reply.sh";
          continue;
        case BOOTREQUEST:
          print_addr(&sa.sa4, got, "BOOTREQUEST %s", bxid((struct bootp *)buf));
          script="./request.sh";
          break;
        }

      memset(decode.pos, 0, sizeof decode.pos);
      print_pkt(buf, got, &decode, DEBUG(RECV));

      if (cleanup)
        {
          cleanup++;
          continue;
        }

      const char *line	= request(script, buf, &got, &decode, &sa.sa4, interface);
      if (!line)
        continue;	/* something failed	*/

      if (reply(script, buf, got, &sa.sa4, line))
        continue;

      /* this assumes everything was perfectly setup
       * from the script!  Including the ARP requests
       * or the interface broadcast address.
       */
      print_pkt(buf, got, &decode, DEBUG(SEND));
      print_addr(&sa.sa4, got, "sending %s", bxid((struct bootp *)buf));
      if (sendto(fd, buf, got, 0, &sa.sa, salen) < 0)
        {
          err("send error");
          continue;
        }

      cleanup	= 1;
   }
}

