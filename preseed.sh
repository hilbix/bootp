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

printf 'ARGS %q\n' "$0" "$@" >&2
set | grep ^SOCAT >&2
