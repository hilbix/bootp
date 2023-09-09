/* DHCP structure
 *
 * Based on:
 * - https://datatracker.ietf.org/doc/html/rfc2131
 * - https://datatracker.ietf.org/doc/html/rfc2132
 * - https://www.iana.org/assignments/bootp-dhcp-parameters/bootp-dhcp-parameters.xhtml
 *
 * This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 *
 * This is terribly incomplete.
 * This may change drastically if I need it.
 *
 * There definitively shoud be some centrally maintained spec
 * which can be transpiled into something like this header.
 * And any other language like a parser generator ..
 */

const char DHCP_MAGIC[] = { 0x63, 0x82, 0x53, 0x63 };

#define	DHCP_IP		4		/* a single IPv4, 4 octets	*/
#define	DHCP_IPS	0		/* list of IPs, 4 octets each	*/
#define	DHCP_IP2	0		/* list of two IPs, 8 octets each	*/
#define	DHCP_DOM	0		/* ASCII domain string	*/
#define	DHCP_PATH	0		/* Unix file path	*/
#define	DHCP_ASCII	0		/* ASCII string	*/
#define	DHCP_BLOB	0		/* binary data	*/
#define	DHCP_ANYTHING	0		/* Anything (binary or structured)	*/
#define	DHCP_8		1		/* 8 bit number	*/
#define	DHCP_8LIST	0		/* 8 bit numbers	*/
#define	DHCP_16		2		/* 16 bit number	*/
#define	DHCP_32		4		/* 16 bit number	*/
#define	DHCP_TYPE	1, DHCPtype	/* see function	*/

/* ptr must be to the complete option, this is
 * 53.1.TYPE
 *
 * ptr[0] == 53
 * ptr[1] == 1
 * ptr[2] == TYPE
 *
 * returns decoded TYPE or NULL if unknown
 */
static const char *
DHCPtype(const unsigned char *ptr)
{
  /* https://www.iana.org/assignments/bootp-dhcp-parameters/bootp-dhcp-parameters.xhtml	*/
  static const char * const types[] =
    { NULL
    , "DISCOVER" 		/* RFC2132	*/
    , "OFFER" 			/* RFC2132	*/
    , "REQUEST"			/* RFC2132	*/
    , "DECLINE"			/* RFC2132	*/
    , "ACK"			/* RFC2132	*/
    , "NAK"			/* RFC2132	*/
    , "RELEASE"			/* RFC2132	*/
    , "INFORM"			/* RFC2132	*/
    /* up to here see also https://datatracker.ietf.org/doc/html/rfc2132	*/
    , "FORCERENEW"		/* RFC3203	*/
    , "LEASEQUERY"		/* RFC4388	*/
    , "LEASEUNASSIGNED"		/* RFC4388	*/
    , "LEASEUNKNOWN"		/* RFC4388	*/
    , "LEASEACTIVE"		/* RFC4388	*/
    , "BULKLEASEQUERY"		/* RFC6926	*/
    , "LEASEQUERYDONE"		/* RFC6926	*/
    , "ACTIVELEASEQUERY"	/* RFC7724	*/
    , "LEASEQUERYSTATUS"	/* RFC7724	*/
    , "TLS"			/* RFC7724	*/
    };

  unsigned index	= ptr[2];
  return index<sizeof types/sizeof *types ? types[index] : 0;
}

/* https://datatracker.ietf.org/doc/html/rfc2132
 * Recommended reading:
 * https://datatracker.ietf.org/doc/html/rfc3442
 */
struct DHCPoptions
  {
    const char	*name;
    int		n;
    const char *(*fn)(const unsigned char *s);
  } DHCPoptions[] =
  { { "NOP"		, -1	}		/*  0: */
  , { "MASK"		, DHCP_IP	}	/*  1: Client interface netmask (for ->yiaddr)	*/
  , { "TZ"		, DHCP_32	}	/*  2: */
  , { "routers"		, DHCP_IPS	}	/*  3: list of IPv4 routers in the network (usually default gateway)	*/
  , { "times"		, 0	}		/*  4: */
  , { "names"		, 0	}		/*  5: */
  , { "DNS"		, DHCP_IPS	}	/*  6: List of IPv4 DNS servers	*/
  , { "logs"		, 0	}		/*  7: */
  , { "cookies"		, 0	}		/*  8: */
  , { "LPR"		, 0	}		/*  9: */
  , { "impress"		, 0	}		/* 10: */
  , { "RLP"		, 0	}		/* 11: */
  , { "Name"		, DHCP_ASCII	}	/* 12: HOSTNAME	*/
  , { "bootlen"		, 2	}		/* 13: */
  , { "dumpfile"	, 0	}		/* 14: */
  , { "domain"		, DHCP_ASCII	}	/* 15: DOMAINNAME	*/
  , { "swap"		, 4	}		/* 16: */
  , { "root"		, 0	}		/* 17: */
  , { "extension"	, 0	}		/* 18: */
  , { "routing"		, 0	}		/* 19: */
  , { "srcrouting"	, 1	}		/* 20: */
  , { "srcfilter"	, 0	}		/* 21: */
  , { "maxudp"		, 2	}		/* 22: */
  , { "TTLudp"		, 1	}		/* 23: */
  , { "PMTU"		, 4	}		/* 24: */
  , { "PMTUtable"	, 0	}		/* 25: */
  , { "MTU"		, 2	}		/* 26: */
  , { "local"		, 1	}		/* 27: */
  , { "broadcast"	, DHCP_IP	}	/* 28: Client interface broadcast address (for ->yiaddr)	*/
  , { "maskget"		, DHCP_8	}	/* 29: perform subnet mask discovery via ICMP: default 0=no 1=yes	*/
  , { "maskout"		, DHCP_8	}	/* 30: provide subnet mask discovery via ICMP: default 0=no 1=yes	*/
  , { "routerget"	, DHCP_8	}	/* 31: perform RFC1256 router discovery: default 0=no 1=yes	*/
  , { "routersol"	, DHCP_IP	}	/* 32: router discovery address	*/
  , { "routes"		, DHCP_IP2	}	/* 33: static routes, IP/32+Router, IP=0.0.0.0 not allowed	*/
  , { "trailers"	, 1	}		/* 34: */
  , { "arptimeout"	, 4	}		/* 35: */
  , { "ethencaps"	, 1	}		/* 36: */
  , { "TTLtcp"		, 1	}		/* 37: */
  , { "keepalive"	, 4	}		/* 38: */
  , { "TCPgarbage"	, DHCP_8	}	/* 39: TCP Keepalive Garbage (0=no 1=send garbage octet)	*/
  , { "NISdom"		, DHCP_ASCII	}	/* 40: */
  , { "NISips"		, DHCP_IPS	}	/* 41: */
  , { "NTP"		, DHCP_IPS	}	/* 42: NTP servers	*/
  , { "vendor"		, DHCP_ANYTHING	}	/* 43: should be fields of CODE.LEN.DATA[LEN]	*/
  , { "NBNS"		, DHCP_IPS	}	/* 44: NetBIOS over TCP/IP Name Servers	*/
  , { "NBDD"		, DHCP_IPS	}	/* 45: NetBIOS over TCP/IP Datagram Distribution Servers	*/
  , { "NBnodeType"	, DHCP_8	}	/* 46: NetBIOS over TCP/IP Node Type (Bit 0=B 1=P 2=M 3=H)	*/
  , { "NBscopeOpt"	, DHCP_ANYTHING	}	/* 47: NetBIOS over TCP/IP Scope, see RFC1001/1002	*/
  , { "X11font"		, DHCP_IPS	}	/* 48: X Window Font Servers	*/
  , { "X11dm"		, DHCP_IPS	}	/* 49: Machines running xdm	*/
  , { "Requested"	, DHCP_IP	}	/* 50: DISCOVER: requested IP address	*/
  , { "LeaseTime"	, DHCP_32	}	/* 51: DISCOVER,REQUEST,OFFER: requested lease time	*/
  , { "overload"	, DHCP_8	}	/* 52: bits: 0 = file 1 = sname	*/
  , { "DHCPtype"	, DHCP_TYPE	}	/* 53: packet type, see function DHCPtype()	*/
  , { "DHCPserver"	, DHCP_IP	}	/* 54: +OFFER+REQUEST,ACK,NAK: server address	*/
  , { "DHCPparam"	, DHCP_8LIST	}	/* 55: DHCP codes wanted by client	*/
  , { "DHCPmessage"	, DHCP_ASCII	}	/* 56: NAK,DECLINE: ASCII message	*/
  , { "maxsize"		, DHCP_16	}	/* 57: maximum DHCP message size	*/
  , { "renew"		, DHCP_32	}	/* 58: RENEWING (sec)	*/
  , { "rebind"		, DHCP_32	}	/* 59: REBINDING (sec)	*/
  , { "vendorcls"	, DHCP_ANYTHING	}	/* 60: from client: server response in 43	*/
  , { "clientId"	, DHCP_BLOB	}	/* 61: from client: unique ID (ignored, we use the MAC)	*/
  , { "NetWareDom"	, DHCP_ASCII	}	/* 62: RFC2242	*/
  , { "NetWareOpt"	, DHCP_ANYTHING	}	/* 63: RFC2242	*/
  , { "NISplusDOM"	, DHCP_DOM	}	/* 64: NIS+ domain	*/
  , { "NISplusIPS"	, DHCP_IPS	}	/* 65: NIS+ servers	*/
  , { "DHCPTFTP"	, DHCP_PATH	}	/* 66: when 'sname' is used for DCHP options (52 overload bit 1)	*/
  , { "DHCPBOOT"	, DHCP_PATH	}	/* 67: when 'file' is used for DHCP options (52 overload bit 0)		*/
  , { "HomeAgents"	, DHCP_IPS	}	/* 68: 0 to n Home Agents (?) listed in preference order	*/
  , { "SMTP"		, DHCP_IPS	}	/* 69: SMTP servers	*/
  , { "POP3"		, DHCP_IPS	}	/* 70: POP3 servers	*/
  , { "NNTP"		, DHCP_IPS	}	/* 71: NNTP servers	*/
  , { "WWW"		, DHCP_IPS	}	/* 72: Web servers	*/
  , { "FINGER"		, DHCP_IPS	}	/* 73: Finger servers	*/
  , { "IRC"		, DHCP_IPS	}	/* 74: IRC servers	*/
  , { "DHCPST"		, DHCP_IPS	}	/* 75: StreeTalk servers	*/
  , { "DHCPSTDA"	, DHCP_IPS	}	/* 76: StreeTalk Directory Assistance servers	*/

#define	DHCP_LAST_KNOWN_OPTION	76

  /* 77-223 see https://www.iana.org/assignments/bootp-dhcp-parameters/bootp-dhcp-parameters.xhtm
   *
   * 100 PCode: see https://datatracker.ietf.org/doc/html/rfc4833
   * 101 TCode: see https://datatracker.ietf.org/doc/html/rfc4833
   * 121 CIDR: see https://datatracker.ietf.org/doc/html/rfc3442
   *	5-N classless route option entries 5-9: CIDR + IPv4portion + routerIPv4 (router=0 if local subnet)
   */
  /* 224 to 254: free to use locally / vendor specific	*/
  };

