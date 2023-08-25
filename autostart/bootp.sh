#!/bin/bash

me="$(readlink -e -- "$0")" || exit
cd "${me%/*/*}" || exit

DEV="${0##*/}"
DEV="${DEV%.sh}"
DEV="${DEV#"${DEV%-*}"}"
DEV="${DEV#-}"

PATH=$PATH:/usr/sbin:/sbin
while	printf '\n%(%Y%m%d-%H%M%S)T rc=%d %q %q\n' -1 "$?" "$PWD" "$DEV" && ! read -t1
do
	# see https://github.com/hilbix/run-until-change
	run-until-change bootp -- ./bootp "${DEV:-vmbr0}"
done
