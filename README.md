> Semi-Automated install of ProxMox and `virsh` VMs works.
> Besides that it still may be a bit incomplete.
> 
> It has matured a bit, so I do no more expect drastic changes.
> But this is no promise.
>
> I use it in on my production.

[![bootp Build Status](https://api.cirrus-ci.com/github/hilbix/bootp.svg?branch=master)](https://cirrus-ci.com/github/hilbix/bootp/master)


# Shell based ~~PXE~~ boot service

I really do not get it.  I tried ISC dhcpd and failed.  I tried dnsmasq and failed.  I tried bootpd and failed.
Because all software around the boot process is overly complex, unflexible and does not fit my needs!

Booting is as simple as it can get.  The protocol ist stable for over 30 years or so.
But nothing really usable (read: scriptable) there, yet.

So I had to create my own.

> Currently I do not use it with PXE/TFTP.
>
> To install a Debian VM from CDROM installer, all you need to do is to prime
> the console with `a` (Return) `a` (Return).  The rest then is fully automatic.
>
> The important part is, that it does not only work for a single VM,
> but all(!) parameters can be given from the ProxMox UI (or `virt-manager`)
> if you like (just by editing a snapshot's comment).
>
> Also `generic.preseed` works for me (for Debian, Ubuntu not yet tested).


## Usage

	sudo apt install jq build-essential

	git clone https://github.com/hilbix/bootp.git
	cd bootp
	git submodule update --init --recursive
	make

	./bootp interface

Now try to boot something on the interface.

### `./request.sh`

> Skip to section "Default `request.sh`" if you are not interested about internals.

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

The DHCP packet is passed to the script in STDIN.  The offset are into this packet.
The DHCP options in the request are passed via environment.  This is a bit hacky:

- `DHCP${OPT}_len=N` length (in bytes) of the DHCP option
- `DHCP${OPT}_pos=N` position (byte offset) of the DHCP option in the DHCP data
- `DHCP${OPT}_bytes=XX XX XX XX` up to 4 bytes of the DHCP option in network byte order
- DHCP option `${OPT}` ranges from `00` to `ff`, but only for existing options

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
	SECS seconds
	FLAG flags
	WANT cia.d.d.r

If some command (except `DHCP`) is given multiply, the last setting survives.

- `VEND` is the vendor extension.
  - As this is not really supported, just use `VEND 0` for now
  - Without `VEND 0` and without `DHCP` the vendor part will be copied from the request (which makes the answer fail)
  - The script currently has no access to the vendor (in future it might be piped in from STDIN)
  - `DHCP` uses the `VEND` extension, too, so both are mutually exclusive
- `ADDR ip` sets the IP address (BOOTP `yiaddr`) to send to the client
  - This corresponds to `$IP_ASSIGNED` (or `${10}`)
  - Use `ADDR 0.0.0.0` to ignore the request
- `HOST name` sets the reported server hostname (BOOTP `sname`)
  - Corresponds to `$NAME` (or `$1`)
- `TFTP ip` sets the "next hop" (BOOTP `siaddr`)
  - Corresponds to `$IP_TFTP` (or `${11}`)
- `FILE name` sets the TFTP filename (BOOTP `file`)
  - Corresponds to `$FILE` (or `$8`)
- `GATE ip` sets the gateway IP (BOOTP `giaddr`)
  - Corresponds to `$IP_GW` (or `${12}`)
- `WANT ip` sets the "wanted" IP (BOOTP `ciaddr`)
- `SECS seconds` sets the seconds from boot field (BOOTP `secs`)
- `FLAG flags` sets the flag field (BOOTP `flags`)
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
  - `x`: some string in hex writing (with or without spaces between the bytes) (untested)

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

You also have the full power of the Shell using `set -x` in scripts like `request.sh`.

Also `request.sh` saves the variables into `cache/$IP.ip` and sets a softlink to this
as `cache/$MAC.mac`, such that you can diagnose, what was really created by `request/*.sh`
and possibly augmented by `ip/$IP.sh`.  Note that `preseed.sh` relies on this information.

> To debug/recreate the `cache/$IP.ip`:
>
> ```
> rm -f cache/$IP.ip
> ssh $USER@$IP sudo dhclient -v
> ```
>
> This sends a BOOTP request and triggers `request.sh` to create the removed cache file again


## Default `preseed.sh`

Serving Debian Preseed files usually is troublesome:

- You need to create them properly
- You need to serve them properly

With the help of `preseed.sh` this should become easy:

- `./preseed.sh` listens on `127.0.0.1:8901` by default
  - It (somehow) understands the PROXY protocol version 1 of HaProxy
- `./preseed.sh 9999,bind=1.2.3.4` would listen on `1.2.3.4:9999`
  - The syntax stems from `socat tcp-listen:PORT,bind=HOST`
- `autostart/preseed.sh` is meant for autostarting it [my way](https://github.com/hilbix/ptybuffer/blob/master/script/autostart.sh)
  - `ln -s --relative autostart/preseed.sh ~/autostart/preseed-192.168.1.254.sh` for `192.168.1.254:80`
  - `ln -s --relative autostart/preseed.sh ~/autostart/preseed-192.168.1.254:8080.sh` for `192.168.1.254:8080`
  - `ln -s --relative autostart/preseed.sh ~/autostart/preseed-:8080.sh` for port `8080` on all interfaces
  - and so on.

`/etc/haproxy/haproxy.conf`:
```
listen  http
        bind    192.168.0.1:80	# put your internal interface here
        mode    http
        server  preseed 127.0.0.1:8901 send-proxy
```

> If you are not familiar with HaProxy, get in touch with it.  It is extremely useful.

`preseed.sh` will convert `/d-i/SEEDNAME/preseed.cfg` into `preseed/SEEDNAME.preseed`
and will fall back to `SEEDNAME.preseed`.  So if you use `_SEED=generic` in the VM settings
(snapshot comment), it will serve the default `generic.preseed`.

This uses following variables to populate the preseed:

- `_HOSTNAME` and `_DOMAINNAME` - which are also used for DHCP
- `_USERNAME` and `_PASSWORD` for the user
- `_APTPROXY` for the apt proxy URL

You can easily extend this using files in `preseed/` directory.
Just use a softlink to put this directory elsewhere.

Note that `preseed.sh` is able to run on port 80, too.  However for this it must run as `root`.


## Default `request.sh`

The default `request.sh` includes all scripts in the `request/` directory
until one sets the variable `$IP`.  For example see [request/proxmox.sh](request/proxmox.sh)


### Snapshot comment format

The best place I found so far is to put everyting into the description of a snapshot.

This works well with `virsh` and ProxMox without need to alter any part of UIs etc.

- Create a VM
- Create a snapshot
- Put the settings, line by line, into the description of the snapshot
  - All lines are of the form `_VAR=VALUE`
  - Other lines are ignored

The snapshots are read from `current` upwards to the root, until one which sets the `_IPv4` is found.
Other snapshots which are not on the straight path are ignored.

Older values do not overwrite newer ones.
- Except for `_DHCP`, which are combined

Following `_TAG`s are recognized (see [request/proxmox.sh](request/proxmox.sh)):

- `_IPv4=a.b.c.d` sets the IP.  Search stops after this snapshot.
- `_HOSTNAME=myhostname` for setting DHCP hostname
- `_DOMAINNAME=example.com` for setting DHCP domainname
- `_DNS4=1.1.1.1 8.8.8.8` or similar for setting DHCP DNS servers
- `_FILE=name` sets the boot filename
- `_SEED=codename` sets a special `_FILE`: `http://$_GW/d-i/$SEED/preseed.cfg`
  - `$_GW` is automatically determined from the `IPv4` given
- `_DHCP` outputs a DHCP option
  - see `DHCP id type data` above
  - see `struct DHCPoptions` in [dhcp.h](dhcp.h)
- `_GW` and so on
- You can create your own variables for use with `preseed.sh`
  - These can be used in the `.preseed` templates
  - An example template is `generic.preseed`

> This also can be extended to your needs in [request/proxmox.sh](request/proxmox.sh)
> if you dare.

You can set variables for use in `preseed.sh`, too:

```
_IPv4=192.168.1.3
_SEED=generic
_HOSTNAME=myhostname
_DOMAINNAME=example.com
_DNS4=1.1.1.1 8.8.8.8
_USERNAME=user
_PASSWORD=pw
_APTPROXY=http://192.168.1.1:3142
```

With these additional parameters (starting with `_USERNAME`), `preseed.sh` can then automatically fill `generic.preseed`
or any other `preseed/$NAME.preseed` file.  Note that `preseed/generic.preseed` takes precedence over `./generic.preseed`.

Also a script named `ip/192.168.1.3.sh` can be used for additional variable manipulation.

- Do not forget to prefix the variables with `_` (as in the snapshot comment)
- `ip/192.168.1._.sh` is tried as fallback
- `ip/192.168._._.sh` is tried as fallback of the fallback
- `ip/192._._._.sh` is tried as last resort fallback

Use a softlink as `ip/` to pull in things from another (parental) `git` repo etc.

Note that the script in `ip/` can overwrite all variables set by `request/*.sh` script.
So be sure to use something like following if you want to set some defaults there:

```
_USERNAME="${_USERNAME:-defaultuser}"
_PASSWORD="${_PASSWORD:-defaultpassword}"
```

Side note:  The variables pulled by `preseed.sh` are pulled from `cache/`.
The latter is filled by `./request.sh`, so be sure after altering `ip/`
to do at least one DHCP request by the VM to regenerate the variables
for a VM if looking at the preseed file.

> You can create such a DHCP request with something like
>
> `dhcpcd -T` or `dhcpcd -T eth0` or similar.
>
> Interestingly this crashes with a SIGSEGV at my side,
> but it successfully does the DHCP request.

You can debug the preseed file from the VM with something like:

```
curl "http://$(ip -j r g 0.0.0.1 | jq -r .[].gateway)/d-i/generic/preseed.cfg"
```


### `request/proxmox.sh`

This needs `jq` and `qm` for accessing ProxMox.
It detects the presence of ProxMox and does nothing if ProxMox is not available.

### `request/virsh.sh`

This needs `virsh`, `jq` and Python3.  To read XML and convert it to JSON it uses `xml2json/` submodule.

> Perhaps other virtualizations can be added a similar way.
> Contributions welcome, as long as you drop all Copyright on them so I can put it under CLL.


## Example

> This example assumes
> - your `virsh` internal interface (`vmbr0`) has IP `192.168.1.1`
>   - or your ProxMox internal interface (`virbr0`) has IP `192.168.1.1` accordingly
> - On `192.168.1.1:3142` some `apt-cacher-ng` runs
>   - If you do not want this, leave `APTPROXY` away
>   - however then the interface must allow NAT to allow the VM to access Internet directly
> - On `192.168.1.1:80` HaProxy listens and forwards requests to `127.0.0.1:8901` (see above)
> - On the interface no other service like `DHCP` runs

Define a VM.

Set it to boot from `debian-12.1.0-amd64-netinst.iso`
- probably downloaded from <https://cdimage.debian.org/cdimage/release/12.1.0/amd64/iso-cd/>
- **Do not forget to verify it with `SHA512SUMS` authenticated via `SHA512SUMS.sign`**
- For a script to do authenticated downloads from some existing Debian, see <https://github.com/hilbix/download-debian>

Do not start it

As long as it is off, define a first snapshot
- Name it `INSTALL` (or what you suits best)

Put in the description:

```
_IPv4=192.168.1.2
_SEED=generic
_DNS4=1.1.1.1 8.8.8.8
_HOSTNAME=test
_DOMAINNAME=example.com
_USERNAME=myuser
_PASSWORD=somesecret
_APTPROXY=http://192.168.1.1:3142
```

Run `./preseed.sh` (and keep it running!)
- Without HaProxy run `./preseed.sh 80,bind=192.168.1.1` as root

Run `./bootp vmbr0` as root (and keep it running!)
- If this exec fails, you probably already have running some DHCP service on the interface.
- Hence you need to run this on some interface which has no DHCP or other things.
- To [autostart this my way](https://github.com/hilbix/ptybuffer/blob/master/script/autostart.sh)
  use something like `ln -s --relative autostart/bootp.sh ~/autostart/boot-vmbr0.sh`

Open console of the VM
- Start the VM
- In the console press `a` (Return) `a` (Return)

The install should now run and finish automatically.

Notes:

- You can use `_FILE` instead of `_SEED` to give the URL you like (untested)
- I haven't done it yet, but it is possible to create a script to fully automate the install
  - Define a VM
  - Setup snapshot as described above
  - Start the VM
  - Wait a bit (perhaps we can even detect some console activity)
  - Send `a` (Return) `a` (Return) to the console
  - Wait for the VM downloading the preseed file
  - Wait for the VM so stop
  - If something takes too long, signal error (or stop VM and retry)
- You can further customize things with some (softlinked) directories
  - `ip/$IP.sh` shell script which can set or manipulate `_*` variables for the IP
  - `preseed/$_SEED.preseed` to give your own PRSEEED file to `./preseed.sh`


## FAQ

WTF why?

- Because I got angry

License?

- This Works is placed under the terms of the Copyright Less License,  
  see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
- Read: Free as free beer, free speech and free baby.

Interface?

- Use one of the output of  
  `ip -j a s  | jq -r '.[].ifname'`
- `bootp` can be run on different interfaces in parallel

TFTP?

- Probably never done
  - It works for me [with CDROM installs](https://github.com/hilbix/download-debian)
- Not sure if it would be done here or in separate repo

IPv6?

- This is BOOTP/DHCP.  Hence it is only for IPv4.

Softlinks?

- I do not like to configure things in complex configuration files
- Instead I like to use softlinks and filenames to express configuration
  - Hence the way I autostart things, etc., just by placing a softlink
- This also works very well with configuration management like Ansible
- On Windows I run my things in WSL, which works extremely well
  - But I doubt HyperV can be easily added to this, too.
  - Note that I like to be proven wrong ;)

Why not CloudInit?

- This here allows to boot or install from authentic sources.
- Also I need something, which works in an offline lab situation.
  - All examples I was able to find only referred to some unauthenticated downloads of some more or less untrusted cloud sources.
  - Exactly 0 of them used the [authenticated mimimum netinstaller `.iso` download](https://github.com/hilbix/download-debian) of Debian out of the box
  - If anybody can enlighten me how to run things with CloudInit without overly complex pre-setup phase and breaking the trust chain, I would be very, very glad!
  - **I do not understend why enforcing the trustchain isn't the default requirement for everything we invent today!**
- Note that I do not even trust what is provided by my own computers.
  - Hence I even require from my own services to enforce the trust chain.
  - Always.
- So I tried to grok how to do install securely with CloudInit
  - I had a look at many examples found out there.
  - But all failed miserably for me.
  - Hence it was easier and faster for me to create this here.
- To upgrade the install process must be easy
  - Just download the newest ISO
  - Authenticate the ISO
  - Boot into it
  - Install with it
- AFAICS CloudInit needs an overly complex preparation of the standard `.iso`
  - And this process greatly varies from case to case and `.iso` to `.iso`
  - Hence nobody can expect, that things done today will continue to work tomorrow.
  - Then you have to find out how and why.
  - Do you have the time to do so?  I don't!
- In contrast, this here should continue to work with minimal adaptions
  - Because I do not expect that BOOTP, DHCP or Preseed see drastic changes
- The best I found was <https://ubuntu.com/server/docs/install/autoinstall-quickstart> so far
  - But I dit not manage to adopt this to `virsh` (`virt-manager`) and ProxMox (UI).
  - And I did not manage to adopt this to other distros like Debian.
  - Perhaps I am just too dumb to do so.

Contact?  Contrib?

- Open Issue or PR on GitHub
- Eventually I listen
- Waive your Copyright, else I cannot use your contribution!

Bugs?  TODOs?

- `bootp` is currently a bit picky and sloppy at the same time when it comes to proper formats
  - The request script is responsible to hand out proper `DHCP` codes
- Be sure to restart `bootp` if it fails
  - It currently relies to the default signal handling
  - It also OOPSes on things it does not understand
- `_SEED` to set the distribution is stupid and should be autodetectable from `ISO`
  - However that is not a `bootp` limitation, it is just not yet scripted
  - You can add it yourself with a bit of scripting if you like
  - Or vice versa, create some script which sets the ISO based on the `_SEED` value
  - To get the seed run a DHCP request and then `cat cache/$IP.ip` or `cat cache/$MAC.mac`
- `VEND` handling should be better
  - But now that `DHCP` is supported, I do not think it is not worth improving
- Error processing/output could be improved
  - Especially debugging
  - Use the power of shell (AKA `set -x`)
- Some fields are not handed to the script:
  - hops
  - perhaps some other
- Some fields cannot be changed yet
  - hops
  - perhaps some other
- The script names are hardcoded
  - `request.sh` for requests
  - `reply.sh` for processing replies
  - `reply.sh` is not implemented (as I have found no use for it yet)
- Default ports (67 and 68) are hardcoded
- Broadcast address use is hardcoded
- Broadcast response is partly hardcoded
  - This is due to flags not present in script yet.
  - Hence the script cannot decide ..
- Barely tested for now
- This is a bit Debian centric
  - and Proxmox
  - and `virsh`
  - as I use it this way
- This is not meant to stick to the DHCP standard
  - It is only meant to be easily hackable to just do what you want
  - So if you break it, you are left alone with all the (missing) parts
- This could be better documented
  - However why as long as I am the lonely one using it ..

