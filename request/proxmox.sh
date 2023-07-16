#
# ProxMox VM autodetection
#
# The very line of the description of a VM must contain the Boot config
#	IPv4 file
# file must not contain %

[ -d /etc/pve/local/qemu-server ] || return

CONFS=(/etc/pve/local/qemu-server/*.conf)

ok=false
for vm in $(grep -il "^net.*=$MAC," "${CONFS[@]}")
do
	VM="${vm##*/}"
	VM="${VM%.*}"

	conf="$(qm config "$VM" --current 1)"

	case "${conf,,}" in
	(*$'\nnet'*"=$MAC,"*)	;;
	(*)			continue;;
	esac

	desc="${conf#*$'\ndescription: '}"
	desc="${desc%%$'\n'*}"
	desc="${desc%%'%'*}"

	ip="${desc%% *}"

	case "$ip" in
	(*.*.*.*) ;;
	(*)	continue;
	esac

	file="${desc#"$ip"}"
	file="${file# }"

	ok=:

	printf 'VM %q %q %q\n' "$VM" "$ip" "$file" >&2
	break
done

$ok && IP="$ip"

