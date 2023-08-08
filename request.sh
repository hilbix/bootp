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
INFILE="$8"		# packet: file name
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

for a in request/*.sh
do
	# '' for default
	IP=		# MUST [ip4] interface IPv4.  '-': script did it, 0.0.0.0 to ignore this MAC
	MASK=		# OPT: [mask] interface netmask, either /CIDR or 255.x.y.z
	GW=		# OPT: [ip4] default router
	LEASE=		# OPT: [s] until RENEWING,  default 1000000
	REBIND=		# OPT: [s] until REBINDING, default 1500000
	TFTP=		# OPT: [ip4] TFTP, default: GW
	FILE=		# OPT: [path] BOOTP-Path
	SEED=		# OPT: [word] http://$GW/d-i/$SEED/preseed.cfg
	SECS=		# OPT: [s] seconds since boot
	FLAG=		# OPT: flags
	REPL=		# OPT: "port IP" or "0 IP" (for default port) reply to address
	. "$a"
	request "$@"
	[ -n "$IP" ] && break
done
case "$IP" in
(-)		exit;;	# script did everything
('')		bye no matching VM found 'for' "$MAC";;
(0.0.0.0)	bye ignoring "$MAC";;
esac

run()
{
  local -n ___VAR___="$1"

  [ -n "$___VAR___" ] && return
  ___VAR___="$("${@:2}")" || bye $?: run "$@"
}

def()
{
  local -n ___VAR___="$1"

  [ -n "$___VAR___" ] && return
  [ 2 -ge $# ] || printf -v ___VAR___ ' %q' "${@:3}"
  printf -v ___VAR___ %s%s "$2" "$___VAR___"
}

run	GW_J_R	ip -j r g "$IP"
run	GW_IP	jq -r '.[0].prefsrc' <<<"$GW_J_R"
run	GW_DEV	jq -r '.[0].dev' <<<"$GW_J_R"
run	GW_J_A	ip -j a s "$GW_DEV"
run	GW_CIDR	jq -r '.[].addr_info[] | select(.family=="inet").prefixlen' <<<"$GW_J_A"
def	MASK	"/$GW_CIDR"
def	GW	"$GW_IP"
def	TFTP	"$GW"
def	SEED	''
def	FILE	"${SEED:+http://$GW/d-i/$SEED/preseed.cfg}"
def	REPL	''
def	FLAG	0
def	SECS	0
def	LEASE	1000000
def	REBIND	1500000
case "$MASK" in
(/*)	bits=$[ 0xffffffff << (32 - "${MASK#/}") ];
	printf -v MASK '%d.%d.%d.%d' $[ (bits>>24)&0xff ] $[ (bits>>16)&0xff ] $[ (bits>>8)&0xff ] $[ (bits>>0)&0xff ];
	;;
esac

out()
{
  case "${@:$#}" in
  (''|-)	;;
  (*)		printf %s "$1"; printf ' %s' "${@:2}"; printf '\n';;
  esac
}

# XXX TODO XXX verify DHCP request for validity

{
echo "$NAME $GW_IP IF $INTERFACE MAC $MAC IP $IP"
arp -d "$IP"
arp -s -i "$INTERFACE" "$IP" "$ARP" temp
} >&2

out REPL "$REPL"
out ADDR "$IP"				# Set the IP of the VM
out TFTP "$TFTP"			# Set our interface as TFTP server
out HOST "$GW_IP"			# output BOOTP/DHCP server IP
out FILE "$FILE"			# output boot file
out SECS "$SECS"
out FLAG "$FLAG"

case "$DHCP53_bytes" in
(01)	out DHCP 53 1 02;;	# DISCOVER => OFFER
(03)	out DHCP 53 1 05;;	# REQUEST => ACK    -- we probably should be a bit more clever here
(*)	out VEND 0; printf 'unsupported DHCP type %q\n' "$DHCP53_bytes" >&2; exit 0;;
esac
out DHCP 54 i "$GW"
out DHCP  1 i "$MASK"	# => malformed according to tshark?
out DHCP  3 i "$GW"	# works
out DHCP 58 4 "$LEASE"
out DHCP 59 4 "$REBIND"
out DHCP 255

#sleep 5
exit 0

