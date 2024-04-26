#!/bin/bash
#
# This is an example bootp script.
# request/*.sh are the scripts to autodetect the IP.
#
# ip/$IP.sh are additional scripts to parse and augment according to $IP
# (see also ./preseed.sh)
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

[ -d cache ] || mkdir cache

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

macbug()
{
  touch cache/BUG "cache/$MAC.bug"
  rm -vf "cache/$MAC.mac" >&2
  bye "$@" "$MAC"
}

# remove stale (older) files using find
# min can be a fraction (undocumented feature of find -mmin)
: stale min files opt..
stale()
{
  find "${@:2}" -mmin "+$1" -delete -printf 'stale %p\n' >&2
}

[ -z "$SERV" ] || [ ".$SERV" = ".$NAME" ] || bye forwarding not yet implemented

# Some helpers for request() below

# Walk the snapshots via: SNAP=$($1); $2 "$SNAP"; SNAP=$($3 "$SNAP")
# returns 0 if found and data was output (else nothing is output)
walksnaps()
{
  local SNAP tag data vals more nr

  [ 3 -ge "$#" ] || printf %q "$4" >&2
  [ 4 -ge "$#" ] || printf ' %q' "${@:5}" >&2
  [ 3 -ge "$#" ] || printf \\n >&2

  SNAP="$($1 0)"
  nr=0
  while	[ -n "$SNAP" ] || return
	while	IFS== read -ru6 tag data		# _TAG=VALUE
	do
		#printf '%q %d %q\n' "$VM" "$tag" "$data" >&2
		: "$VM" "$tag" "$data"
		case "$tag" in
		(_DHCP)		dhcp+=("$data");;	# remember DHCP, only output if IP found
		(_IPv4)		IP="$data";;		# IP is handled a special way
		([^_]*)		;;			# _TAG must start with _
		(_[^A-Z]*)	;;			# _TAG must start with _X where X is uppercase letter
		(*[^A-Z0-9_]*)	;;			# _TAG must be made of uppercase/numbers/underscore
		(?*)		[ -n "${!tag}" ] || eval "$tag=\"\$data\"";;	# standard VARs
		esac
	done 6< <($2 "$nr" "$SNAP" && echo)

	: "$IP"
	case "$IP" in
	(*.*.*.*)	break;;
	esac
  do
	let nr++
	[ 100 -gt $nr ] || return
	SNAP="$($3 "$nr" "$SNAP")"
  done

  [ 0 = "${#dhcp[@]}" ] || printf 'DHCP %s\n' "${dhcp[@]}"
}

run()
{
  local -n ___VAR___="$1"	# this is correct, no _ here

  [ -n "$___VAR___" ] && return
  ___VAR___="$("${@:2}")" || bye $?: run "$@"
}

def()
{
  local -n ___VAR___="_$1"	# this is correct, _ added in front

  [ -n "$___VAR___" ] && return
  [ 2 -ge $# ] || printf -v ___VAR___ ' %q' "${@:3}"
  printf -v ___VAR___ %s%s "$2" "$___VAR___"
#  printf '_%q = %q\n' "$1" "$___VAR___" >&2
}

out()
{
  case "${@:$#}" in
  (''|-)	;;
  (*)		printf %s "$1"; printf ' %s' "${@:2}"; printf '\n';;
  esac
}

# XXX TODO XXX verify DHCP request for validity

arp()
{
  echo "$NAME $GW_IP IF $INTERFACE MAC $MAC IP $IP"
  /usr/sbin/arp -d "$IP"
  /usr/sbin/arp -s -i "$INTERFACE" "$IP" "$ARP" temp
}

# Augment a bit
augment()
{
  local a
  for a in "$1" "${1%.*}._" "${1%.*.*}._._" "${1%.*.*.*}._._._" "${1%"${1#*-}"}_._._._"
  do
	[ -s "ip/$a.sh" ] || continue
	pushd ip >/dev/null &&
	. "./$a.sh"
	popd >/dev/null
	#break	Do not break here, also read more generic ones
  done
}

# This is run in a subshell to protect against case where something goes horribly wrong.
# The result (cache/$MAC.mac) must be imported later on
request()
{
  # remove possibly stale data
  rm -vf "cache/$MAC.mac" >&2

  # stamp that we did evaluation.  Do it prematurely
  # in case somethings goes horribly wrong in the next steps
  echo "$MAC $IP" > cache/LAST

  # All cachable variables, except IP, start with _
  for a in request/*.sh
  do
	# '' for default
	unset -- ${!_*}
	IP=		# MUST [ip4] interface IPv4.  '-': script did it, 0.0.0.0 to ignore this MAC
	_MASK=		# OPT: [mask] interface netmask, either /CIDR or 255.x.y.z
	_GW=		# OPT: [ip4] default router
	_BC=		# OPT: [ip4] Broadcast address, default taken from interface
	_RENEW=		# OPT: [s] until RENEWING,  default  900000
	_LEASE=		# OPT: [s] for LEASE time,  default 1000000
	_REBIND=	# OPT: [s] until REBINDING, default 1500000
	_TFTP=		# OPT: [ip4] TFTP, default: GW
	_FILE=		# OPT: [path] BOOTP-Path
	_SEED=		# OPT: [word] http://$GW/d-i/$SEED/preseed.cfg
	_SECS=		# OPT: [s] seconds since boot
	_FLAG=		# OPT: flags
	_REPL=		# OPT: "port IP" or "0 IP" (for default port) reply to address
	_HOSTNAME=	# OPT: hostname (DHCP 12)
	_DOMAINNAME=	# OPT: domain name (DHCP 15)
	_DNS4=		# OPT: list of IPv4 nameservers (DHCP 6)

	. "$a" &&	# import request()
	request "$@" &&	# run request()
	[ -n "$IP" ] &&
	break
  done

  run	IF_J_A	ip -j -4 a s dev "$INTERFACE"
  run	IF_IPS	jq -r '.[].addr_info[].local' <<<"$IF_J_A"

  # Try to autodetect the IP if still missing
  if	read -ra IF_IPs <<<"$IF_IPS" && [ -z "$IP" ]
  then
	printf 'autodetect IP for %q from' "$MAC" >&2
	printf ' %s' "${IF_IPs[@]}" >&2
	printf '\n' >&2
#	for debug in NAME INTERFACE ARP FROM MAC TXN SERV; do printf '%10q=%q\n' "$debug" "${!debug}"; done >&2
	for IF_IP in "${IF_IPs[@]}"
	do
		augment "auto-$IF_IP"
	done
  fi

  case "$IP" in
  (-)		exit;;	# script did everything
  ('')		macbug no IP found 'for';;
  (0.0.0.0)	macbug ignoring;;
  esac

  run	GW_J_R	ip -j r g "$IP"
  run	GW_IP	jq -r '.[0].prefsrc' <<<"$GW_J_R"
  run	GW_DEV	jq -r '.[0].dev' <<<"$GW_J_R"
  run	GW_J_A	ip -j a s "$GW_DEV"
  run	GW_CIDR	jq -r '.[].addr_info[] | select(.family=="inet").prefixlen' <<<"$GW_J_A"
  def	MASK	"/$GW_CIDR"
  def	GW	"$GW_IP"
  def	TFTP	"$_GW"
  def	SEED	''
  def	REPL	''
  def	FLAG	0
  def	SECS	0
  def	RENEW	 900000
  def	LEASE	1000000
  def	REBIND	1500000
  case "$_MASK" in
  (/*)	bits=$[ 0xffffffff << (32 - "${_MASK#/}") ];
	printf -v _MASK '%d.%d.%d.%d' $[ (bits>>24)&0xff ] $[ (bits>>16)&0xff ] $[ (bits>>8)&0xff ] $[ (bits>>0)&0xff ];
	;;
  esac
  IFS=. read a b c d u v w x <<<"$IP.$_MASK" && printf -v BCDEF %d.%d.%d.%d $[ a|(u^255) ] $[ b|(v^255) ] $[ c|(w^255) ] $[ d|(x^255) ]
  def	BC	"$BCDEF"

  augment "$IP"

  def	FILE	"${_SEED:+http://$_GW/d-i/$_SEED/preseed.cfg}"

  # Do some caching (for preseed.sh etc.)
  for c in IP ${!_*}
  do
	[ _ = "$c" ] || printf "%q=%q\n" "$c" "${!c}"
  done > "cache/$IP.tmp" &&			# output cache/$IP.cache
  mv -f "cache/$IP.tmp" "cache/$IP.ip" &&	# this hopefully is an atomic rename()
  ln -fs "$IP.ip" "cache/$MAC.mac"		# create the result

  # repeat the stamp
  echo "$MAC $IP" > cache/LAST
}

# Pile up BUGs for 12s
if	[ -e cache/BUG ]
then
	stale 0.2 cache/BUG
else
	# remove all bugs such that all bugged VMs re-evaluate
	rm -vf cache/*.bug >&2
fi

# Ignore bugged MACs
[ -e "cache/$MAC.bug" ] && bye temporarily ignored "$MAC"

# pile up subsequent bugs a bit longer,
# such that working MACs have a chance to come through
[ -e cache/BUG ] && touch cache/BUG

# Pile up working requests for 24s
# this assumes that there are periods of 24s where no
# other requests (except for BUGs) arrive.
stale 0.4 cache/LAST
# remove outdated MACs (older than 1 minute) if no more piling up
[ -e cache/LAST ] || stale 1 cache/ -name '*.mac'
# reuse last MAC request, such that the lengthy discovery process is not repeated
if	[ ! -s "cache/$MAC.mac" ] || [ ! -e "cache/$MAC.dhcp" ]
then
	printf 'calculating request: %q\n' "$MAC" >&2
	# This is single threaded, so there are no concurrent evaluations going on here
	(
	[ -e "cache/$MAC.trace" ] && set -x
	time -p request
	) >"cache/$MAC.dhcp" || macbug request processing failed with "$?" 'for'
else
	printf 'using cached request: %q\n' "$MAC" >&2
fi

# import the result
[ -s "cache/$MAC.mac" ] && . "cache/$MAC.mac" || macbug invalid or stale data 'for'
cat "cache/$MAC.dhcp"

arp >&2

case "$DHCP53_bytes" in
(01)	out DHCP 53 1 2;;	# DISCOVER => OFFER
(03)	out DHCP 53 1 5;;	# REQUEST => ACK    -- we probably should be a bit more clever here
(07)	bye ignoring DHCP NOTIFY type "$DHCP53_bytes";;
(*)	bye unsupported DHCP type "$DHCP53_bytes";;
esac

out REPL "$_REPL"
out ADDR "$IP"			# Set the IP of the VM
out TFTP "$_TFTP"		# Set our interface as TFTP server
out HOST "$_GW_IP"		# output BOOTP/DHCP server IP
out FILE "$_FILE"		# output boot file
out SECS "$_SECS"
out FLAG "$_FLAG"

out DHCP 12 s "$_HOSTNAME"
out DHCP 15 s "$_DOMAINNAME"
out DHCP  6 i "$_DNS4"
out DHCP 28 i "$_BC"
out DHCP 54 i "$_GW"
out DHCP  1 i "$_MASK"		# => malformed according to tshark?
out DHCP  3 i "$_GW"		# works
out DHCP 58 4 "$_RENEW"
out DHCP 51 4 "$_LEASE"
out DHCP 59 4 "$_REBIND"
out DHCP 255

exit 0

