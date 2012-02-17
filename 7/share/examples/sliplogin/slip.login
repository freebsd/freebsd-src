#!/bin/sh -
#
#	@(#)slip.login	8.1 (Berkeley) 6/6/93

#
# generic login file for a slip line.  sliplogin invokes this with
# the parameters:
#      1        2         3        4          5         6     7-n
#   slipunit ttyspeed loginname local-addr remote-addr mask opt-args
#
# Delete any arp table entries for this site, just in case
/usr/sbin/arp -d $5
# Bringup the line
/sbin/ifconfig sl$1 inet $4 $5 netmask $6
# Answer ARP request for the SLIP client with our Ethernet addr
# XXX - Must be filled in with the ethernet address of the local machine
# /usr/sbin/arp -s $5 00:00:c0:50:b9:0a pub
exit
