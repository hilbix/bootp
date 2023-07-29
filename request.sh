#!/bin/bash
#
# This is an example bootp script.
# request/*.sh are the scripts to autodetect the IP
#
# This needs "jq", so: sudo apt install jq


#printf 'ARG: %q\n' "$@" >&2
#printf 'DHCP_TYPE=%q\n' "$DHCP53_bytes" >&2
#set | grep ^DHCP >&2

NAME="$1"		# hostname (uname -n)
INTERFACE="$2"		# interface we use
ARP="$3"		# socket: MAC (Currently faked from $MAC!)
FROM="$4"		# socket: IP
MAC="$5"		# packet: MAC from packet
TXN="$6"		# packet: transaction number from packet (unchangeable)
SERV="$7"		# packet: server name (if set we are not authoritative)
FILE="$8"		# packet: file name
IP_WANTED="$9"		# packet: wanted IP
IP_ASSIGNED="${10}"	# packet: assigned IP (i.E. from proxied answers)
IP_TFTP="${11}"		# packet: TFTP IP (next hop)
IP_GW="${12}"		# packet: Gateway IP (forwarder)

bye()
{
echo ADDR 0.0.0.0	# setting 0 as address suppresses answer
{
printf fail
printf ' %q' "$@"
printf '\n'
} >&2
exit
}

[ -z "$SERV" ] || [ ".$SERV" = ".$NAME" ] || bye forwarding not yet implemented

IP=
for a in request/*.sh
do
	. "$a"
	[ -n "$IP" ] && break
done
[ -n "$IP" ] || bye no matching VM found 'for' "$MAC"

#printf 'ARG %q\n' "$@" >&2

IP="$ip"
GWA="$(ip -j r g "$IP")"
GW="$(jq -r '.[0].prefsrc' <<<"$GWA")"
GWDEV="$(jq -r '.[0].dev' <<<"$GWA")"
GWBITS="$(ip -j a s "$GWDEV" | jq '.[].addr_info[] | select(.family=="inet").prefixlen')"
GWbits=$[ 0xffffffff << (32 - GWBITS) ];
case "$DHCP53_bytes" in
(01)	printf 'DHCP 53 1 02\n';;
(03)	printf 'DHCP 53 1 05\n';;
esac
#printf 'DHCP 1 %d.%d.%d.%d\n' $[ (GWbits>>24)&0xff ] $[ (GWbits>>16)&0xff ] $[ (GWbits>>8)&0xff ] $[ (GWbits>>0)&0xff ];
printf 'DHCP 54 i %q\n' "$GW"
printf 'DHCP 51 4 %d\n' 1000000
printf 'DHCP 255\n'

{
echo "$NAME $GW IF $INTERFACE MAC $MAC"
arp -d "$IP"
arp -s -i "$INTERFACE" "$IP" "$ARP" temp
} >&2

#echo "SECS 0"
echo "ADDR $IP"				# Set the IP of the VM
echo "TFTP $GW"				# Set our interface as TFTP server
echo "HOST $GW"
[ -z "$file" ] || echo "FILE $file"	# set the boot file

#sleep 5
exit 0

