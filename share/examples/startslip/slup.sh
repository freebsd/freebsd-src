#!/bin/sh
myname=<my.ip.address>
gateway=<gateway.ip.address>
netmask=255.255.255.248
tune="link0 -link2"     # links means force compression

/sbin/ifconfig $1 $2 $tune
/sbin/ifconfig $1 inet $myname $gateway netmask $netmask
/sbin/route add default $gateway
