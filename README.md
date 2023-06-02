# shell based PXE boot service

I really do not get it.  I tried ISC dhcpd and failed.  I tried dnsmasq and failed.  I tried bootp and failed.

The software all around a boot process is overly complex, unflexible and cannot be used at all!
WTF why?  Booting is as simple as it can get.  The protocol ist stable for over 30 years.
But nothing usable there, yet, really?

This here to the rescue, and it is easy.  Sorry for all the hassle.  But future becomes bright.


## Usage

	git clone https://github.com/hilbix/pxeboot.git
	cd pxeboot
	make

	./pxeboot interface

Try to boot something on the interface.
On stdout you will see what to do.
Just follow the white rabbit.
Things could not be more easy than that.


## Troubleshooting

Run `tcpdump` on the system:

	tcpdump -npi any udp

- Look if you see BOOTP packets.
- If there are none, this here cannot work.

Check your firewall.

`tcpdump` shows packets before they hit netfilter.  But if the firewall blocks the packets, they will not reach the user space.

	iptables save | grep -w 67

shows following (manually configured) entry at my side:

	-A PVEFW-HOST-IN -s 0.0.0.0/32 -d 255.255.255.255/32 -i vmbr0 -p udp -m udp --sport 68 --dport 67 -j RETURN


## FAQ

WTF why?

- Because I got angry

License?

- Free as free beer, free speech and free baby.

