#!/bin/sh -
# $FreeBSD: src/sbin/ip6fw/sample.sh,v 1.1.2.1 2000/05/04 17:35:00 phantom Exp $

fwcmd=/sbin/ip6fw

$fwcmd -f flush

#
# loopback
#
$fwcmd add 1000 pass      all from any to any via lo0

#
# ND
#
# DAD
$fwcmd add 2000 pass ipv6-icmp from ff02::/16 to ::
$fwcmd add 2100 pass ipv6-icmp from :: to ff02::/16
# RS, RA, NS, NA, redirect...
$fwcmd add 2300 pass ipv6-icmp from fe80::/10 to fe80::/10
$fwcmd add 2400 pass ipv6-icmp from fe80::/10 to ff02::/16

$fwcmd add 5000 pass tcp from any to any established

# RIPng
$fwcmd add 6000 pass udp from fe80::/10 521 to ff02::9 521

$fwcmd add 65000 pass log all from any to any
