#
# vim: ft=bash
#
# ProxMox VM autodetection
#
# This looks the snapshot chain upwards and reads data from description:
#
# IPv4 IP4
# SEED codename
# DHCP id type data
#
# It stops looking if IPv4 is found.
# Unrecognized lines are ignored.
# codename is translated to http://IP/d-i/codename/preseed.cfg
# if SEED is given multiply, the most recent snapshot wins
#
# Recommended DHCP options:
#
# DHCP 6 i 1.1.1.1 8.8.8.8
# DHCP 29 1 0

[ -d /etc/pve/local/qemu-server ] || return

CONFS=(/etc/pve/local/qemu-server/*.conf)

get-ip()
{
  local depth desc tag data conf="$(qm config "$1" --current 1)" snap=()

  case "${conf,,}" in
  (*$'\nnet'*"$2"*)	;;
  (*)			return;;
  esac

  snap=()
  while IFS='' read -r line
  do
        depth="${line%%[^ ]*}"
        depth="${#depth}"
        line="${line#*'-> '}"
        name="${line%% *}"
        snap[$depth]="$name"
        [ current = "$name" ] && break
  done < <(LC_ALL=C qm listsnapshot "$1")

  [ current = "${snap[$depth]}" ] &&
  while	let depth--
  do
        conf="$(qm config "$1" --snapshot "${snap[$depth]}")" || return
        desc="${conf#*$'\ndescription: '}"	# extract description
        [ ".$conf" = ".$desc" ] && continue
        desc="${desc%%$'\n'*}"			# decode it
        printf -v desc '%b' "${desc//%/\\x}"	# decode it
        while	read -ru6 tag data
        do
#		printf '%q %d %q %q %q\n' "$VM" "$depth" "${snap[$depth]}" "$tag" "$data" >&2
                case "$tag" in
                (SEED|FILE)	l="${tag,,}"; [ -n "${!l}" ] || eval "$l=\"\$data\"";;
                (DHCP)		dhcp+=("$data");;
                (IPv4)		ip="$data";;
                esac
        done 6<<<"$desc"
        [ -n "$ip" ] && return
  done
  return 1
}

request()
{
  local VM vm ip file seed dhcp=()

  ok=false
  for vm in $(grep -il "^net.*=$MAC," "${CONFS[@]}")
  do
        VM="${vm##*/}"
        VM="${VM%.*}"

        get-ip "$VM" "=$MAC," || continue

        case "$ip" in
        (*.*.*.*) ;;
        (*)	continue;
        esac

        ok=:
        printf 'VM %q %q %q\n' "$VM" "$ip" "${file:-$seed}" >&2
        break
  done

  $ok || return

  [ 0 = "${#dhcp[@]}" ] || printf 'DHCP %s\n' "${dhcp[@]}"

  IP="$ip"
  FILE="$file"
  SEED="$seed"
}

