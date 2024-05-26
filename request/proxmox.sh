#
# vim: ft=bash
#
# ProxMox VM autodetection
#
# This looks the snapshot chain upwards and reads data from description:
# It stops looking if IPv4 is found.

[ -d /etc/pve/local/qemu-server ] || return

#set -x

CONFS=(/etc/pve/local/qemu-server/*.conf)

# This runs as a subshell
snapinfo()
{
#  printf 'DEBUG: %q arg %q\n' "$@" >&2
  if	[ 0 = "$1" ]
  then
	# $2 == 'current' here
	# Dump _PROXMOX_ variables from $conf got in request()
	mapfile -t PROXMOX <<<"$conf" &&
	for p in "${PROXMOX[@]}"
	do
		pk="${p%%:*}"
		pk="${pk%% *}"
		case "$pk" in
		(*[^a-z0-9A-Z_]*)	printf 'WTF %q: %q\n' "$0" "$p" >&2;;
		([a-z]*)		printf '_PROXMOX_%s=%s\n' "${pk^^}" "${p#"$pk: "}"
#					printf 'DEBUG %q: %q %q\n' "$pk" "${p#"$pk: "}" >&2
					;;
		esac
	done
	return
  fi

  conf="$(qm config "$VM" --snapshot "$2")" || return
  desc="${conf#*$'\ndescription: '}"	# extract description
  [ ".$conf" = ".$desc" ] && return	# not found
  desc="${desc%%$'\n'*}"		# remove trailing lines

  printf '%b' "${desc//%/\\x}"		# decode description
}
snapnext()
{
  [ "${1:-0}" -le "$depth" ] && printf '%s' "${snap[$[depth-$1]]}"
}
snapget()
{
   snap=()
   while IFS='' read -r line
   do
	depth="${line%%[^ ]*}"
	depth="${#depth}"
	line="${line#*'-> '}"
	name="${line%% *}"
	snap[$depth]="$name"
	[ current = "$name" ] && return
   done < <(LC_ALL=C qm listsnapshot "$VM")
   return 1
}

request()
{
  local conf snap depth

  while read -ru6 VM
  do
	VM="${VM##*/}"
	VM="${VM%.*}"

	conf="$(qm config "$VM" --current 1)"
	case "${conf,,}" in
	(*$'\nnet'*"=$MAC,"*)	;;
	(*)			continue;;
	esac

	snapget || continue
	# inject $conf for snapinfo 0
	walksnaps snapnext snapinfo snapnext proxmox VM "$VM"
	# This is an success, even if IP is not set yet
	return 0
  done 6< <(grep -il "^net.*=$MAC," "${CONFS[@]}")
  return 1
}

