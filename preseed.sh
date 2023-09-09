#!/bin/bash
#
# Assemble preseed.cfg
#
# Run as:
#
# ./preseed.sh PORT
#
# Expects HTTP request on STDIN for http://$GW/d-i/codename/preseed.cfg
# Output HTTP answer for it on STDOUT

[ -n "$SOCAT_VERSION" ] || exec socat -dd tcp-listen:"${1:-8901}",reuseaddr,fork exec:"$0" || exit

LC_ALL=C	# we need to count bytes, not UTF-8 characters

reply()
{
printf 'HTTP/1.0 %03d %s\r\n' "$1" "$2"
printf 'content-type: text/plain\r\n'
printf 'content-length: %d\n' $[1+${#content}]
printf 'connection: close\r\n'
printf '\r\n'
echo "$content"
}

err()
{
content="$*"
reply "$1" "${*:2}"
printf 'ERR: %q\n' "$from $*" >&2
exit 1
}

from="$SOCAT_PEERADDR"

read -t10 -r GET URL HTTP || err 408 Timeout
if	[ 127.0.0.1 = "$from" -a PROXY = "$GET" ]
then
        # Allow HaProxy Proxy Protocol version 1 (send-proxy, not send-proxy-v2), but only from localhost
        from="${HTTP%% *}"
        read -t10 -r GET URL HTTP || err 408 Timeout
fi
[ GET = "$GET" ] || err 405 Only GET allowed
case "$URL" in
(/*/*/*/*)		err 414 path too long;;
(/d-i/*/preseed.cfg)	;;
(*)			err 404 unknown;;
esac
while	read -rt5 x || err 408 Timeout
        [ -n "${x%$'\r'}" ]
do :; done

printf '\n%q %q %q %q\n\n' "$from" "$GET" "$URL" "$HTTP" >&2

what="${URL#/d-i/}"
what="${what%/preseed.cfg}"

unset IP ${!_*}
[ -s "cache/$from.ip" ] && . "cache/$from.ip" || err 404 unknown "$from"

SRC="preseed/$what.preseed"
[ -s "$SRC" ] || SRC="$what.preseed"
[ -s "$SRC" ] || err 404 not found "$what"

LINES=()
while read -ru6 -a line
do
        case "$line" in
        (''|'#'*)	continue;;
        esac
        v="${line[-1]}"
        case "$v" in
        ('$'*[^A-Z0-9_]*)	;;
        ('$'[_A-Z]*)		v="_${v#?}"; v="${!v}"; [ -z "$v" ] && continue; line[-1]="$v";;
        esac
        LINES+=("${line[*]}")
done 6<"$SRC"

printf -v content '%s\n' "${LINES[@]}"
reply 200 OK

