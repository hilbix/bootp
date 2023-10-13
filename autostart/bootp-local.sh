#!/bin/bash
# same as bootp.sh but for /usr/local/bin/bootp with SUID flags
#
# ln -s --relative bootp-localbin.sh ~/autostart/bootp-virbr0.sh

me="$(readlink -e -- "$0")" || exit
cd "${me%/*/*}" || exit

DEV="${0##*/}"
DEV="${DEV%.sh}"
DEV="${DEV#"${DEV%-*}"}"
DEV="${DEV#-}"

PATH="$PATH:/usr/sbin:/sbin"
while	printf '\n%(%Y%m%d-%H%M%S)T rc=%d %q %q\n' -1 "$?" "$PWD" "$DEV" && ! read -t1
do
	/usr/local/bin/bootp "${DEV:-vmbr0}"
done

