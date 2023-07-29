# BOOTP

Total payload length is 300 to 548 bytes:

| length | type    | name   | description                            |
|--------|---------|--------|----------------------------------------|
| 1      | enum    | op     | 1:BOOTREQUEST, 2:BOOTREPLY             |
| 1      | enum    | htype  | 1:Ethernet, 6:IEEE802, 7 ARCNET        |
| 1      | length  | hlen   | 6:Ethernet                             |
| 1      | count   | hops   | 0:number of relay agents               |
| 4      | id      | xid    | 32 bit transaction ID                  |
| 2      | uint32  | secs   | seconds since client start             |
| 2      | flags   | flags  | RFC1542: MSB:use broadcast reply       |
| 4      | IPv4    | ciaddr | wanted IP                              |
| 4      | IPv4    | yiaddr | assigned IP                            |
| 4      | IPv4    | siaddr | TFTP server                            |
| 4      | IPv4    | giaddr | relay agent                            |
| 16     | bytes   | chaddr | MAC or address of hlen                 |
| 64     | cstring | sname  | wanted/replying server name            |
| 128    | cstring | file   | PXE filename                           |
| 64-312 | bytes   | vend   | vendor-specific area                   |

Everything is in network byte order.


# BOOTP-Vendor / DHCP

- <https://datatracker.ietf.org/doc/html/rfc1048>
- <https://datatracker.ietf.org/doc/html/rfc2131>
- <https://datatracker.ietf.org/doc/html/rfc2132>
- <http://www.tcpipguide.com/free/t_SummaryOfDHCPOptionsBOOTPVendorInformationFields-2.htm>

- Cookie:
  - `63 82 53 63`

- `R` are the relevant options

| dec | code | LEN  | type     | Name       | R | Description                             |
|-----|------|------|----------|------------|---|-----------------------------------------|
|   0 | 00   | 0    | byte     | PAD        | x |                                         |
|   1 | 01   | 4    | IPv4     | Mask       | x | Subnet Mask                             |
|   2 | 02   | 4    | int32    | Time       |   | Time zone offset in seconds             |
|   3 | 03 N | N    | IPv4list | routers    |   | routers                                 |
|   4 | 04 N | N    | IPv4list | times      |   | RFC868 timeservers (not: NTP)           |
|   5 | 05 N | N    | IPv4list | names      |   | IEN116 name servers                     |
|   6 | 06 N | N    | IPv4list | DNS        |   | STD13 DNS                               |
|   7 | 07 N | N    | IPv4list | logs       |   | MIT-LCS UDP logservers                  |
|   8 | 08 N | N    | IPv4list | cookies    |   | RFC865 cookie servers                   |
|   9 | 09 N | N    | IPv4list | LPR        |   | RFC1179 LPR                             |
|  10 | 0A N | N    | IPv4list | impress    |   | Imagen Impress servers                  |
|  11 | 0B N | N    | IPv4list | RLP        |   | RFC887 Resource Location Servers        |
|  12 | 0C N | N    | string   | Name       |   | hostname/fqdn of client                 |
|  13 | 0D N | 2    | uint16   | bootlen    |   | length/512 of boot file                 |
|  14 | 0E N | N    | string   | dumpfile   |   | path of dumpfile when crashing          |
|  15 | 0F N | N    | string   | domain     |   | domain name of client                   |
|  16 | 10 N | 4    | IPv4     | swap       |   | IP of swap server                       |
|  17 | 11 N | N    | string   | root       |   | path on root server (NFS, etc.)         |
|  18 | 12 N | N    | bytes    | extension  |   | vendor-specific data                    |
|  19 | 13 N | 1    | bool     | routing    |   | 0=off 1=on enable routing on client     |
|  20 | 14 N | 1    | bool     | srcrouting |   | 0=off 1=allow non-local source routing  |
|  21 | 15 N | N    | IPv4mask | srcfilter  |   | non-local source-routed datagrams       |
|  22 | 16 N | 2    | uint16   | maxudp     |   | maximum datagram reassembly size >=576  |
|  23 | 17 N | 1    | uint8    | TTLudp     |   | default TTL for outgoing UDP            |
|  24 | 18 N | 4    | uint32   | PMTU       |   | seconds in aging for PMTU discovery     |
|  25 | 19 N | N    | uint16s  | PMTUtable  |   | value table for PMTU discovery          |
|  26 | 1A N | 2    | uint16   | MTU        |   | MTU for interface >= 68 (dec)           |
|  27 | 1B N | 1    | bool     | local      |   | 1=same MTU on all subnets 0=may be less |
|  28 | 1C N | 4    | IPv4     | broadcast  | x | broadcast address                       |
|  29 | 1D N | 1    | bool     | maskget    |   | 0=dont 1=use ICMP for mask discovery    |
|  30 | 1E N | 1    | bool     | maskout    |   | 0=dont 1=answer ICMP for mask discovery |
|  31 | 1F N | 1    | bool     | routerget  |   | 0=dont 1=do ICMP router discovery       |
|  32 | 20 N | 4    | IPv4     | routersol  |   | address for router solicitation         |
|  33 | 21 N | N    | routes   | routes     |   | IPv4 static routes                      |
|  34 | 22 N | 1    | bool     | trailers   |   | 0=dont 1=use RFC 893 trailers           |
|  35 | 23 N | 4    | uint32   | arptimeout |   | seconds for arp cache timeout           |
|  36 | 24 N | 1    | uint8    | ethencaps  |   | 0=RFC894 ethII 1=RFC1042 802.3          |
|  37 | 25 N | 1    | uint8    | TTLtcp     |   | default TTL for outgoing TCP            |
|  38 | 26 N | 4    | uint32   | keepalive  |   | seconds for keepalife, 0=no/application |
|  39 | 27 N | 1    | bool     | garbage    |   | 1=keepalive with garbage byte (compat)  |
|  40 | 28 N | N    | string   | NISdomain  |   | NIS domain (see also: 40)               |

|     | 35 N | 1    | byte     | DHCPtype   |   | DHCP Message Type (see below)           |
|     | 36 N | 4    | IPv4     | DHCPtype   |   | DHCP server IP (DHCPREQUEST: selected)  |
|     | 37 N | N    | bytes    | DHCPparam  |   | Parameter Request List                  |
|     | 38 N | N    | string   | message    |   | error message etc.                      |

|  64 | 40 N | N    | string   | NISdomain  |   | NIS+ domain (see also: 28)              |

|  68 | xx N | N    | unknown  | reserved   |   | reserved fur future definition          |
| 128 | 8x N | N    | unknown  | sitedata   |   | site-specific information               |
| 144 | 9x N | N    | unknown  | sitedata   |   | site-specific information               |
| 160 | Ax N | N    | unknown  | sitedata   |   | site-specific information               |
| 176 | Bx N | N    | unknown  | sitedata   |   | site-specific information               |
| 192 | Cx N | N    | unknown  | sitedata   |   | site-specific information               |
| 208 | Dx N | N    | unknown  | sitedata   |   | site-specific information               |
| 224 | Ex N | N    | unknown  | sitedata   |   | site-specific information               |
| 240 | Fx N | N    | unknown  | sitedata   |   | site-specific information               |
| 255 | FF   | 0    | byte     | END        |   | end of options                          |

TODO:
- <http://www.tcpipguide.com/free/t_SummaryOfDHCPOptionsBOOTPVendorInformationFields-5.htm>

- IPv4list:
  - multiple of 4 byte
  - each 4 bytes is IPv4 in network byte order
- IPv4mask:
  - multiple of 8 byte
  - first 4 bytes are IPv4 in network byte order
  - last 4 bytes are IPv4 mask in network byte order
- routes:
  - multiple of 8 byte
  - first 4 bytes are IPv4 of destination in network byte order
  - last 4 bytes are IPv4 of router in network byte order
- DHCPtype
  - `01 C DHCPDISCOVER`
  - `02 S DHCPOFFER`
  - `03 C DHCPREQUEST`
  - `04 C DHCPDECLINE`
  - `05 S DHCPPACK`
  - `06 S DHCPNAK`
  - `07 C DHCPRELEASE`
  - `08 C DHCPINFORM`

