# shell based PXE boot service

I really do not get it.  I tried ISC dhcpd and failed.  I tried dnsmasq and failed.  I tried bootpd and failed.
Because all software around the boot process is overly complex, unflexible and does not fit my needs!

Booting is as simple as it can get.  The protocol ist stable for over 30 years or so.
But nothing really usable there, yet.

So I had to create my own.


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

These commands can give it multiply, the last setting survives.

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

Note about answers:

- Machines doing BOOTP/DHCP/PXE do not respond to ARP requests
- To allow answers, **you must prime the ARP cache first**
- See the example `./request.sh` how to do this

> This minimalism is no bug.
>
> `bootp` was written according to the perfection principle:
>
> Perfection is reached, if you can no more leave anything away.
>
> Handling `arp` can be done by the script, hence is not needed in `bootp`.


## Troubleshooting

Maximum debugging is currently:

	DEBUG=3 ./bootp interface

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


## FAQ

WTF why?

- Because I got angry

TFTP?

- I am working on it
- Not sure if here or in separate repo

License?

- Free as free beer, free speech and free baby.

Bugs?  TODOs?

- `bootp` is currently a bit picky and sloppy at the same time when it comes to proper formats
- Be sure to restart `bootp` it if it fails
  - It currently relies to the default signal handling
  - It also OOPSes on things it does not understand
- VEND handling should be better
  - Currently the script has no access to the VEND data from the request
- Error processing/output could be improved
- Some fields are not handed to the script:
  - RFC1542 flags field
  - second field
  - hops
- Some fields cannot be changed
  - hops
  - `WANT`ed IP
- The script names are hardcoded
  - `request.sh` for requests
  - `reply.sh` for replies
  - Example for `reply.sh` is missing
- Default ports (67 and 68) are hardcoded
- Broadcast address use is hardcoded
- Occasionally you will see something like:  
  `Script ./request.sh must not close stdout before terminating.`
  - I am not sure why `waitpid()` is slower than STDIO
  - Looks like a SMP race condition to me
    (ressources are closed before termination is signalled)
  - Perhaps there must be a minimum timeout added to `waitpid()`
- Broadcast response is hardcoded
  - This is due to flags not present in script yet.
  - Hence the script cannot decide ..
- Barely tested for now
- This is a bit Debian centric
  - and Proxmox
  - as I use it

