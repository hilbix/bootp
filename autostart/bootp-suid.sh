#!/bin/bash
# same as bootp.sh but for `suid` usage when running as non-root
# with installed /usr/local/bin/bootp
# see ../suid.conf.d/suid.conf.d/suid.conf
#
# ln -s --relative bootp-suid.sh ~/autostart/bootp-virbr0.sh

me="$(readlink -e -- "$0")" || exit
cd "${me%/*/*}" || exit

DEV="${0##*/}"
DEV="${DEV%.sh}"
DEV="${DEV#"${DEV%-*}"}"
DEV="${DEV#-}"

PATH="$PATH:/usr/sbin:/sbin"
while	printf '\n%(%Y%m%d-%H%M%S)T rc=%d %q %q\n' -1 "$?" "$PWD" "$DEV" && ! read -t1
do
	suid bootp "${DEV:-vmbr0}"
done

