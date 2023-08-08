#!/bin/bash

me="$(readlink -e -- "$0")" || exit
cd "${me%/*/*}" || exit

PATH=$PATH:/usr/sbin:/sbin
while	printf '\n%(%Y%m%d-%H%M%S)T rc=%d %q\n' -1 "$?" "$PWD" && ! read -t1
do
	run-until-change bootp -- ./bootp vmbr0
done
