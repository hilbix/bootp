/* Allow PXE boot on a certain interface
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

#define	DHCP_MAGIC_COOKIE	0x63825363		/* MSB first! */

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
xd(const char *what, void *ptr, int len)
{
  int	w = 32, i, pos, max, end;

  for (end=len; --end>=0; )
    if (((unsigned char *)ptr)[end])
      break;
  if ((end+=1) > len-4)
    end	= len;

  for (pos=0; pos<len; pos+=w)
    {
      max	= len-pos;
      if (max>w) max = w;

      printf("%s %04x", what, pos);
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
      printf("  ! ");
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

#define	XD(what,elem)	xd(what,&(elem),sizeof (elem))

void
print_pkt(void *buf, int len)
{
  struct bootp *b = (void *)buf;

  if (!(debug&1)) return;

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
  xd("VEND", &b->vend, len - offsetof(struct bootp, vend));
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


/* fork a script with parameters.
 * process output to adapt buffer
 */
char *
request(const char *script, void *buf, int *len, struct sockaddr_in *sa, const char *interface)
{
  struct bootp	*b = buf;
  int		fds[2];
  pid_t		pid;

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
      /* feed buffer to script?	*/
      const char *mac = bmac(b); /* faked vor now --vvv */
      execlp(script, script, hostname(), interface, mac, ip(4, &sa->sin_addr), mac, bxid(b), bsname(b), bfile(b), bip(0, b), bip(1, b), bip(2, b), bip(3, b), NULL);
      err(script);
      exit(-1);
    }
  close(fds[1]);

  FILE	*fd;

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
  ret	= strdup("");

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

      if (debug&2) printf("DEBUG: %s\n", line);

      if (!strncmp("FILE ", line, 5))
        {
          my_strncpy(b->file, line+5, sizeof b->file);
          continue;
        }
      if (!strncmp("HOST ", line, 5))
        {
          my_strncpy(b->sname, line+5, sizeof b->sname);
          continue;
        }
      if (!strncmp("TFTP ", line, 5))
        {
          b->siaddr = fromip(line+5);
          continue;
        }
      if (!strncmp("ADDR ", line, 5))
        {
          b->yiaddr = fromip(line+5);
          continue;
        }
      if (!strncmp("GATE ", line, 5))
        {
          b->giaddr = fromip(line+5);
          continue;
        }
      if (!strncmp("REPL ", line, 5))
        {
          if (ret) free(ret);
          ret	= strdup(line+5);
          continue;
        }
      if (!strncmp("VEND ", line, 5))
        {
          int tmp;

          tmp	 = set_vend(b, line+5);
          if (tmp>0)
            {
              FATAL(tmp < BOOTP_MINSIZE);
              *len	= tmp;
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

  for (;;)
    {
      pid_t	id;
      int	status;

      id	= waitpid(-1, &status, WNOHANG);
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
                return b->yiaddr ? ret : 0;
              printf("script %s failed with code %d\n", script, WEXITSTATUS(status));
            }
          else if (WIFSIGNALED(status))
            printf("script %s terminated with signal %d\n", script, WTERMSIG(status));
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
          printf("Script %s must not close stdout before terminating.\n", script);
          return 0;
        }
    }
}

int
main(int argc, char **argv)
{
  const char	*interface;
  int		fd;
  int		cleanup;

  FATAL(BOOTP_MINSIZE != sizeof(struct bootp));

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
      char		buf[MAX_PKG_LEN];
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
          xd("PKT", buf, got);
          continue;
        }
      if (got < BOOTP_MINSIZE)
        {
          print_addr(&sa.sa4, got, "too short BOOTP");
          xd("PKT", buf, got);
          continue;
        }
      if (buf[1] != 1 || buf[2] != 6)
        {
          print_addr(&sa.sa4, got, "packet frame %2x %2x should be 01 06", buf[0], buf[1]);
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
      print_pkt(buf, got);

      if (cleanup)
        {
          cleanup++;
          continue;
        }

      const char *line	= request(script, buf, &got, &sa.sa4, interface);
      if (!line)
        continue;	/* something failed	*/

      if (reply(script, buf, got, &sa.sa4, line))
        continue;

      /* this assumes everything was perfectly setup
       * from the script!  Including the ARP requests
       * or the interface broadcast address.
       */
      print_addr(&sa.sa4, got, "sending %s", bxid((struct bootp *)buf));
      print_pkt(buf, got);
      if (sendto(fd, buf, got, 0, &sa.sa, salen) < 0)
        {
          err("send error");
          continue;
        }

      cleanup	= 1;
   }
}

