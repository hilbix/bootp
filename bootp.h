/* BOOTP structure
 *
 * See https://datatracker.ietf.org/doc/html/rfc951
 *
 * This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 */

#define	BOOTREQUEST	1
#define	BOOTREPLY	2

#define	BOOTP_MINSIZE	4+4+2+2+4*4+16+64+128+64	/* 300	*/
#define	BOOTP_MAXPKG	2000				/* 576 or MTU	*/

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

