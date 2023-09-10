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

snapinfo()
{
  conf="$(qm config "$VM" --snapshot "$2")" || return
  desc="${conf#*$'\ndescription: '}"	# extract description
  [ ".$conf" = ".$desc" ] && return	# not found
  desc="${desc%%$'\n'*}"		# remove trailing lines
  printf '%b' "${desc//%/\\x}"		# decode it
}
snapnext()
{
  [ "${1:-0}" -le "$depth" ] && printf '%s' "${snap[$[depth-1-$1]]}"
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

        snapget && walksnaps snapnext snapinfo snapnext
        return
  done 6< <(grep -il "^net.*=$MAC," "${CONFS[@]}")
  return 1
}

