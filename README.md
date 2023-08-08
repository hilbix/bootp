> With this I am already able to automatically install ProxMox VMs.  
> Besides that it is terribly incomplete and everything might change in future.
>
> Now that there is [xml2json](https://github.com/hilbix/xml2json), so `jq` can be used to parse XML,
> probably `virtsh` support could be added, too.


# Shell based PXE boot service

I really do not get it.  I tried ISC dhcpd and failed.  I tried dnsmasq and failed.  I tried bootpd and failed.
Because all software around the boot process is overly complex, unflexible and does not fit my needs!

Booting is as simple as it can get.  The protocol ist stable for over 30 years or so.
But nothing really usable (read: scriptable) there, yet.

So I had to create my own.

> Currently I have not tested it with PXE/TFTP.
>
> However on ProxMox an automated install of a Debian VM works as described below.  
> All you need to do is to prime the console with `a` (Return) `a` (Return).
> The rest then is fully automatic.
>
> The important part is, that it does not only work for a single VM,
> it works for all similar VMs, provided that each is handed out the correct
> individual preseed file.  (Sadly Debian requires individual preseed files.)


## Usage

	sudo apt install jq build-essential

	git clone https://github.com/hilbix/bootp.git
	cd bootp
	git submodule update --init --recursive
	make

	./bootp interface

Now try to boot something on the interface.
You will see that a script named `./request.sh` is forked with 12 parameters:

	NAME="$1"               # hostname (uname -n)
	INTERFACE="$2"          # interface we use
	ARP="$3"                # socket: MAC (Currently faked from $MAC!)
	FROM="$4"               # socket: IP
	MAC="$5"                # packet: MAC from packet
	TXN="$6"                # packet: transaction number from packet (unchangeable)
	SERV="$7"               # packet: server name (if set we are not authoritative)
	FILE="$8"               # packet: file name
	IP_WANTED="$9"          # packet: wanted IP
	IP_ASSIGNED="${10}"     # packet: assigned IP (i.E. from proxied answers)
	IP_TFTP="${11}"         # packet: TFTP IP (next hop)
	IP_GW="${12}"           # packet: Gateway IP (forwarder)

The script can send lines to STDOUT to alter the reply:

	VEND 0
	ADDR 0.0.0.0
	ADDR ip.addr.to.assign
	HOST name
	TFTP tftp.ser.ver.ip
	FILE filename-for-TFTP
	GATE ga.te.way.ip
	REPL port
	REPL port ip.to.reply.to
	DHCP id type data

If some command (except `DHCP`) is given multiply, the last setting survives.

- `VEND` is the vendor extension.
  - As this is not really supported, just use `VEND 0` for now
  - Without `VEND 0` the vendor part will be copied from the request (which makes the answer fail)
  - The script currently has no access to the vendor (in future it might be piped in from STDIN)
- `ADDR ip` sets the IP address to send to the client
  - This corresponds to `$IP_ASSIGNED` (or `${10}`)
  - Use `ADDR 0.0.0.0` to ignore the request
- `HOST name` sets the reported server hostname
  - Corresponds to `$NAME` (or `$1`)
- `TFTP ip` sets the "next hop"
  - Corresponds to `$IP_TFTP` (or `${11}`)
- `FILE name` sets the TFTP filename
  - Corresponds to `$FILE` (or `$8`)
- `GATE ip` sets the gateway IP
  - Corresponds to `$IP_GW` (or `${12}`)
- `REPL port` sets the reply port
  - Default: 68
  - set to default if 0
- `REPL port ip` sets the reply port and reply ip
  - Default reply IP is the one from `ADDR`
  - If `port` is 0, it becomes the default: 68
- `DHCP id type data`
  - for `id`s see [`dhcp.h`](dhcp.h)
  - `type` defines what `data` follows:
  - `1`: 1 byte decimal
  - `2`: 2 byte decimal
  - `4`: 4 byte decimal
  - `i`: IPv4 `a.b.c.d` (or a space separated list of such IPv4)
  - `m`: IPv4/MASK `a.b.c.d/w.x.y.z` (space separated list supported.  `/CIDR` not yet supported, sorry)
  - `s`: some string
  - `x`: some string in hex writing (with or without spaces between the bytes)

Note about answers:

- Machines doing BOOTP/DHCP/PXE do not respond to ARP requests
- To allow answers, **you must prime the ARP cache**
  - this is done in [`request.sh`](request.sh)
  - See the example `./request.sh` how to do this

> This minimalism (leave everything to the shell) is no bug.
>
> `bootp` was written according to the perfection principle:
>
> Perfection is reached, if you can no more leave anything away.
>
> Handling `arp` can be done by the script, hence is not needed in `bootp`.


## Troubleshooting

Maximum debugging is currently:

	DEBUG=63 ./bootp interface

Run `tcpdump` on the system:

	tcpdump -npi any udp

- Look if you see BOOTP packets.
- If there are none, this here cannot work.

Check your firewall.

`tcpdump` shows packets before they hit netfilter.  But if the firewall blocks the packets, they will not reach the user space.

	iptables save | grep -w 67

shows following (manually configured) entry at my side (`X` is redacted data):

	-A PVEFW-HOST-IN -s 192.168.X.Y/32 -d 192.168.X.0/24 -i vmbr0 -p udp -m udp --sport 67 --dport 68 -j RETURN
	-A PVEFW-HOST-IN -s 0.0.0.0/32 -d 255.255.255.255/32 -i vmbr0 -p udp -m udp --sport 68 --dport 67 -j RETURN


## Default `request.sh`

The default `request.sh` includes all scripts in the `request/` directory
until one sets the variable `$IP`.  For example see [request/proxmox.sh](request/proxmox.sh)


### `request/proxmox.sh`

Sadly ProxMox seems not to support user defined values to edit such settings from the UI.
The best place I found so far is to put everyting into the description of a snapshot.

- Create a VM
- Create a snapshot
- Put the settings into the description of the snapshot

The snapshots are read from `current` upwards to the root, until one which sets the `IPv4` is found.
Other snapshots which are not on the straight path are ignored.

Older values doe not overwrite newer ones, except for `DHCP`, which is combined.

Lines are in the format `TAG VALUE`.  Lines with unknown TAGs are ignored.  

Currently only these `TAG`s are recognized (see [request/proxmox.sh](request/proxmox.sh)):

- `IPv4 a.b.c.d` sets the IP.  Search stops after this snapshot.
- `FILE` sets the boot filename
- `SEED` sets a special `FILE`: `http://$GW/d-i/$SEED/preseed.cfg`
  - `$GW` is automatically determined from the `IPv4` given
- `DHCP` outputs a DHCP option (see `DHCP id type data` above)

This can be extended to your needs in [request/proxmox.sh](request/proxmox.sh).


#### Example

> This example assumes
> - your ProxMox internal interface (`vmbr0`) has IP `192.168.1.1`
> - On `192.168.1.1:3142` some `apt-cacher-ng` runs
>   - If you do not want this, leave `mirror/http/proxy` out of `preseed.cfg`
>   - however then the interface must allow NAT to allow the VM to access Internet directly
> - On `192.168.1.1:80` some web service runs which is able to server the `preseed.cfg` file
>   - Hence `curl http://192.168.1.1/d-i/bookworm/preseed.cfg` on ProxMox outputs the file
> - On the interface no other service like `DHCP` runs

Define a VM.

- Set it to boot from `debian-12.1.0-amd64-netinst.iso`
  - probably downloaded from <https://cdimage.debian.org/cdimage/release/12.1.0/amd64/iso-cd/>
  - **Do not forget to verify it with `SHA512SUMS` authenticated via `SHA512SUMS.sign`**
  - For a script to do authenticated downloads from some existing Debian, see <https://github.com/hilbix/download-debian>
- Do not start it
- As long as it is of, define the first snapshot
- Name it `INSTALL`
- Put in the description below
- Be sure the `preseed` file is served correctly
- Run `./bootp vmbr0` (and keep it running!)
  - as seen in [`autostart/bootp.sh`](autostart/bootp.sh)
  - this is my way to autostart things via [some crude cron script](https://github.com/hilbix/ptybuffer/blob/master/script/autostart.sh)
  - If this fails you probably already have running some DHCP service on the interface.
  - Hence you need to run this on some interface which has no DHCP or other things.
- Open console
- Start the VM
- In the console press `a` (Return) `a` (Return)
- The install should run through completely

Description of `INSTALL`
```
IPv4 192.168.1.2
SEED bookworm
DHCP  6 i 1.1.1.1 8.8.8.8
DHCP 29 1 0
```

File served via `http://192.168.1.1/d-i/bookworm/preseed.cfg`:
```
# (Before doing tons of gigabyte installs just to query debconf-get-selections --installer)

d-i     netcfg/get_hostname                     string          test
d-i     netcfg/get_domain                       string          example.net

d-i     mirror/http/proxy                       string          http://192.168.1.1:3142

d-i     localechooser/supported-locales         multiselect     en_US.UTF-8, de_DE.UTF-8
d-i     debian-installer/locale                 select          en_US.UTF-8
d-i     keyboard-configuration/xkb-keymap       select          de,us
d-i     passwd/user-fullname                    string          test
d-i     passwd/username                         string          test
d-i     passwd/user-password                    password        test
d-i     passwd/user-password-again              password        test

d-i     netcfg/choose_interface                 select          auto

d-i     passwd/root-login                       boolean         false

d-i     partman-auto/method                     string          lvm
d-i     partman-auto-lvm/guided_size            string          16G
d-i     partman-lvm/confirm_nooverwrite         boolean         true

d-i     partman/choose_partition                select          finish
d-i     partman/confirm                         boolean         true
d-i     partman/confirm_nooverwrite             boolean         true

# stolen from https://gist.github.com/sturadnidge/5841112
tasksel tasksel/first                           multiselect     standard,ssh-server

d-i     mirror/country                          string          manual
d-i     mirror/http/hostname                    string          deb.debian.org
d-i     mirror/http/directory                   string          /debian

d-i     grub-installer/only_debian              boolean         true
d-i     grub-installer/bootdev                  string          default

d-i     finish-install/reboot_in_progress       note    
d-i     debian-installer/exit/poweroff          boolean true
```

Notes:

- Currently I run a customized web script which is able to hand out a VM specific `preseed.cfg`
  based on the IP of the `VM`.
- You can use `FILE` instead of `SEED` to give the URL you like (untested)
- I haven't done it yet, but it is possible to create a script to fully automate the install
  - Define a VM
  - Setup snapshot as described above
  - Start the VM
  - Wait a bit (perhaps we can even detect some console activity)
  - Send `a` (Return) `a` (Return) to the console
  - Wait for the VM downloading the preseed file
  - Wait for the VM so stop
  - If something takes too long, signal error (or stop VM and retry)


## FAQ

WTF why?

- Because I got angry

Interface?

- Use one of the output of  
  `ip -j a s  | jq -r '.[].ifname'`
- `bootp` can be run on different interfaces in parallel

TFTP?

- I am working on it
- Not sure if here or in separate repo

License?

- Free as free beer, free speech and free baby.

Bugs?  TODOs?

- `bootp` is currently a bit picky and sloppy at the same time when it comes to proper formats
  - The request script is responsible to hand out proper `DHCP` codes
- Be sure to restart `bootp` if it fails
  - It currently relies to the default signal handling
  - It also OOPSes on things it does not understand
- `SEED` to set the distribution is stupid and should be autodetectable from `ISO`
  - However that is not a `bootp` limitation, it is just not yet scripted
  - You can add it yourself with a bit of scripting if you like
  - Or vice versa, create some script which sets the ISO based on the `SEED` value
  - To get the seed use something like `source request/proxmox.sh; get-ip $VMID && echo $seed`
- `VEND` handling should be better
  - Currently the script has no access to the VEND data from the request
- Error processing/output could be improved
- Some fields are not handed to the script:
  - RFC1542 flags field
  - second field
  - hops
- Some fields cannot be changed yet
  - hops
  - `WANT`ed IP
- The script names are hardcoded
  - `request.sh` for requests
  - `reply.sh` for replies
  - Example for `reply.sh` is missing
- Default ports (67 and 68) are hardcoded
- Broadcast address use is hardcoded
- Broadcast response is partly hardcoded
  - This is due to flags not present in script yet.
  - Hence the script cannot decide ..
- Barely tested for now
- This is a bit Debian centric
  - and Proxmox
  - as I use it this way
- This is not meant to stick to the DHCP standard
  - It is only meant to be easily hackable to just do what you want
  - So if you break it, you are left alone with all the (missing) parts

