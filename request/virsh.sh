#
# vim: ft=bash
#
# Virsh VM autodetection
#
# Walks the snapshots from current up all parents

[ -d /etc/libvirt/qemu ] || return

virsh()		{ LC_ALL=C.UTF-8 exec /usr/bin/virsh "$@"; }
snapfirst()	{ virsh snapshot-current "$VM" | xml2json/xml2json.py; }
snapinfo()	{ jq -r .domainsnapshot.description <<<"$2"; }
snapnext()
{
  snap="$(jq -r .domainsnapshot.name <<<"$2")" &&
  snap="$(virsh snapshot-parent "$VM" --snapshotname "$snap")" &&
  virsh snapshot-dumpxml "$VM" "$snap" | xml2json/xml2json.py
}

request()
{
  while read -ru6 VM
  do
        virsh domiflist "$VM" | grep -q " $MAC\$" || continue
        walksnaps snapfirst snapinfo snapnext
        return
  done 6< <(virsh list --uuid --state-running)
  return 1
}

