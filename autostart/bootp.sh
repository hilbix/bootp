#!/bin/bash
#
# For root: ln -s --relative bootp.sh /root/autostart/bootp-virbr0.sh
# For non-root see suid-bootp.sh

me="$(readlink -e -- "$0")" || exit
cd "${me%/*/*}" || exit

DEV="${0##*/}"
DEV="${DEV%.sh}"
DEV="${DEV#"${DEV%-*}"}"
DEV="${DEV#-}"

PATH="$PATH:/usr/sbin:/sbin"
while	printf '\n%(%Y%m%d-%H%M%S)T rc=%d %q %q\n' -1 "$?" "$PWD" "$DEV" && ! read -t1
do
	./bootp "${DEV:-vmbr0}"
done

