#!/bin/bash
#
# ln -s --relative preseed.sh ~/autostart/preseed-_8901.sh

me="$(readlink -e -- "$0")" || exit
cd "${me%/*/*}" || exit

bind="${0##*/}"
bind="${bind%.sh}"
bind="${bind#"${bind%%-*}"}"
bind="${bind#-}"
HOST="${bind%[:_]*}"	# cut :PORT
bind="${bind#"$HOST"}"
bind="${bind#[:_]}"
bind="${bind:-80}${HOST:+,bind=}$HOST"

PATH="$PATH:/usr/sbin:/sbin"
while	printf '\n%(%Y%m%d-%H%M%S)T rc=%d %q %q\n' -1 "$?" "$PWD" "$bind" && ! read -t1
do
	./preseed.sh "$bind"
done

