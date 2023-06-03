/* Allow PXE boot on a certain interface
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>

#include <arpa/inet.h>

#define	FATAL(test)	do { if (test) OOPS(#test); } while (0)

void _Noreturn
OOPS(const char *s)
{
  perror(s);
  exit(1);
}

void
err(const char *s)
{
  perror(s);
}

void
dot(const char *s)
{
  fwrite(s, sizeof *s, strlen(s), stdout);
  fflush(stdout);
}

int
udp_bound(const char *interface, const char *host)
{
  struct addrinfo	*ret, *ai, hint;
  int			fd;

  memset(&hint, 0, sizeof hint);
  hint.ai_family	= AF_INET;
  hint.ai_socktype	= SOCK_DGRAM;
  hint.ai_flags		= AI_IDN|AI_PASSIVE;
  if (getaddrinfo(host, "67", &hint, &ret))
    OOPS("cannot resolve");

  for (ai = ret;; ai = ai->ai_next)
    {
      if (!ai)
        OOPS("cannot bind socket");

      if ((fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) < 0)
        OOPS("cannot create socket");

      if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, interface, strlen(interface)) < 0)
        OOPS("cannot limit to interface");

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
    OOPS("cannot limit to interface");

  sa4.sin_family	= AF_INET;
  sa4.sin_port		= htons(port);
  sa4.sin_addr.s_addr	= htonl(INADDR_BROADCAST);

  if (bind(fd, &sa4, sizeof sa4) < 0)
    OOPS("cannot bind to broadcast address");

  return fd;
}

#define	BOOTREQUEST	1
#define	BOOTREPLY	2

#define	BOOTP_MINSIZE	4+4+2+2+4*4+16+64+128+64

struct bootp	/* see http://www.tcpipguide.com/free/t_BOOTPMessageFormat.htm	*/
  {
    unsigned char	op;		/* BOOTREQUEST or BOOTREPLY	*/
    unsigned char	htype;		/* 1:Ethernet	*/
    unsigned char	hlen;		/* 6:Ethernet	*/
    unsigned char	hops;		/* 0:number of hops */
    u_int32_t		xid;		/* 32 bit transaction ID */
    u_int16_t		secs;		/* ignored: seconds since boot began */
    u_int16_t		flags;		/* RFC1542 flags: Bit 0 = use broadcast reply */
    u_int32_t		ciaddr;		/* wanted IP	*/
    u_int32_t		yiaddr;		/* assigned IP	*/
    u_int32_t		siaddr;		/* TFTP server	*/
    u_int32_t		giaddr;		/* relay (0)	*/
    unsigned char	chaddr[16];	/* MAC	*/
    char		sname[64];	/* opt: wanted/replying server name */
    char		file[128];	/* PXE filename	*/
    unsigned char	vend[64];	/* ignored: vendor-specific area */
  };

void
print_addr(struct sockaddr *sa, int salen, const char *what, int len)
{
  const char	*s;
  char		buf[128];

#if 1
  FATAL(sa->sa_family != AF_INET);
  /* WTF why do I have to pass a newly founded wheel here?	*/
  s	= inet_ntop(sa->sa_family, &((struct sockaddr_in *)sa)->sin_addr, buf, sizeof buf);
#else
  FATAL(sa->sa_family != AF_INET6);
  s	= inet_ntop(sa->sa_family, &((struct sockaddr_in6 *)sa)->sin6_addr, buf, sizeof buf);
#endif

  FATAL(!s);

  printf("%s len %d from %s\n", what, len, s);
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

int
main(int argc, char **argv)
{
  const char	*interface;
  int		fd;

  FATAL(BOOTP_MINSIZE != sizeof(struct bootp));

  if (argc!=2)
    OOPS("Usage: pxeboot interface");

  interface	= argv[1];
  fd		= udp_bc(interface, 67);

  for (;;)
    {
      char		buf[2000];
      int		got;
      struct sockaddr	sa;
      unsigned		salen;

      salen	= sizeof sa;
      got = recvfrom(fd, buf, sizeof buf, 0, &sa, &salen);
      if (got < 0)
        {
          err("recv");
          continue;
        }
      if (got > sizeof buf)
        OOPS("packet buffer overrun");

      if (sa.sa_family != AF_INET)
        {
          printf("WTF %d: %x not AF_INET %x\n", got, (int)sa.sa_family, (int)AF_INET);
          xd("ADR", &sa, salen);
          xd("PKT", buf, got);
          continue;
        }
      if (got < BOOTP_MINSIZE)
        {
          print_addr(&sa, salen, "too short BOOTP", got);
          xd("PKT", buf, got);
          continue;
        }
      switch (buf[0])
        {
        case BOOTREPLY:
          print_addr(&sa, salen, "BOOTREPLY", got);
          xd("PKT", buf, got);
          continue;
        default:
          print_addr(&sa, salen, "unknown BOOTP", got);
          continue;
        case BOOTREQUEST:
          break;
        }

      print_addr(&sa, salen, "BOOTREQUEST", got);
      print_pkt(buf, got);
   }
}

