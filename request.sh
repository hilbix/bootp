#!/bin/bash
#
# This is an example boot script for ProxMox
#
# This needs "jq", so: sudo apt install jq


#printf 'ARG: %q\n' "$@" >&2

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
GW="$(ip -j r g "$IP" | jq -r '.[0].prefsrc')"

{
echo "$NAME $GW IF $INTERFACE MAC $MAC"
arp -d "$IP"
arp -s -i "$INTERFACE" "$IP" "$ARP" temp
} >&2

echo "VEND 0"				# Remove Vendor (as we are not ready to process this yet)
echo "ADDR $IP"				# Set the IP of the VM
echo "TFTP $GW"				# Set our interface as TFTP server
[ -z "$file" ] || echo "FILE $file"	# set the boot file

exit 0
