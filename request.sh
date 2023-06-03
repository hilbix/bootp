#!/bin/bash

#printf 'ARG: %q\n' "$@" >&2

NAME="$1"
INTERFACE="$2"
MAC="$3"
IP="$4"

printf 'ARG %q\n' "${@:4}" >&2

IP=192.168.16.7

{
echo "$INTERFACE MAC $MAC"
arp -d "$IP"
arp -s -i "$INTERFACE" "$IP" "$MAC" temp
} >&2

echo ADDR "$IP"
echo TFTP 192.168.16.254
echo FILE pxe
echo VEND 0

exit 0
