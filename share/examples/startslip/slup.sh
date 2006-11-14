#!/bin/sh
myname=<my.ip.address>
gateway=<gateway.ip.address>
netmask=255.255.255.248
tune1="link0 -link2"    # force headers compression
tune2="mtu 296"         # for FreeBSD 1.x host

case $LINE in
	0) tune=$tune1;;        # 1st phone connected
	1) tune=$tune2;;        # 2nd phone connected
	*) tune=;;              # others
esac

/sbin/ifconfig $1 $2 $tune
/sbin/ifconfig $1 inet $myname $gateway netmask $netmask
/sbin/route add default $gateway
