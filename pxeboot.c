#define _GNU_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>

void
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
main(int argc, char **argv)
{
  int			fd, i;
  struct addrinfo	*ret, hint;
  const char		*host;
  struct sockaddr_in	sa4;

  if (argc!=2)
    OOPS("Usage: pxeboot ip.of.inter.face");

  host	= argv[1];

  memset(&hint, 0, sizeof hint);
  hint.ai_family	= AF_UNSPEC;
  hint.ai_flags		= AI_IDN;
  if (getaddrinfo("0.0.0.0", "67", &hint, &ret))
    OOPS("cannot resolve");

  if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))<0)
    OOPS("no socket");
  i	= 1;
  if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &i, sizeof i)<0)
    OOPS("setsockopt");

  sa4.sin_family	= AF_INET;
  sa4.sin_port		= htons(67);
  sa4.sin_addr.s_addr	= htonl(INADDR_BROADCAST);
#if 0
  sa4.sin_addr.s_addr	= htonl(INADDR_ANY);
#endif
  if (bind(fd, &sa4, sizeof sa4)<0)
    OOPS("bind");

  for (;;)
    {
      char		buf[2000];
      int		got;
      struct sockaddr	c;
      unsigned		clen;


      got = recvfrom(fd, buf, sizeof buf, 0, &c, &clen);
      if (got < 0)
        {
          err("recv");
          continue;
        }
      dot(".");
   }
}

