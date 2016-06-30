#!/bin/sh
#-
# Copyright (c) 2016 The FreeBSD Foundation
# All rights reserved.
#
# This software was developed by Björn Zeeb under
# the sponsorship from the FreeBSD Foundation.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

if test ! -x /usr/local/bin/netserver -o ! -x /usr/local/bin/netperf ; then
	echo "ERROR: Cannot find netserver or netperf in /usr/local/bin" >&2
	exit 1
fi

case `id -u` in
0)	;;
*)	echo "ERROR: Must be run as superuser." >&2
	exit 2
esac

prefix=/tmp/`date +%s`

i=0

ending()
{
#	vmstat -m > ${prefix}-vmstat-m-2
#	echo "diff -up ${prefix}-vmstat-m-?"
#	vmstat -z > ${prefix}-vmstat-z-2
#	echo "diff -up ${prefix}-vmstat-z-?"

	exit 0
}

wait_enter()
{
	local line
	
#	echo -n "Press Enter to continue.."
#	read line
}

epair_base()
{
	local ep
	
	ep=`ifconfig epair create`
	expr ${ep} : '\(.*\).'
}

# Pre-start a single jail to get rid of various debugging
# output on console for the real test. Make sure not to leak zones but
#  to forcefully clear them or stats are bogus.
#sysctl security.jail.vimage_debug_memory=2
id=`jail -i -c vnet persist`
jail -r ${id}


# Private debugging.
# 0x01 == print; 0x02 destroy uma zones upon jail teardown no matter what
#sysctl security.jail.vimage_debug_memory=0
#sysctl security.jail.vimage_debug_memory=1
#sysctl security.jail.vimage_debug_memory=2
#sysctl security.jail.vimage_debug_memory=3

#vmstat -z > ${prefix}-vmstat-z-1
#vmstat -m > ${prefix}-vmstat-m-1

# Start left (client) jail.
ljid=`jail -i -c host.hostname=left$$.example.net vnet persist`

# Start middle (forwarding) jail.
mjid=`jail -i -c host.hostname=center$$.example.net vnet persist`

# Start right (server) jail.
rjid=`jail -i -c host.hostname=right$$.example.net vnet persist`

echo "left ${ljid}   middle ${mjid}    right ${rjid}"

# Create networking.
#
# jail:		left            middle           right
# ifaces:	lmep:a ---- lmep:b  mrep:a ---- mrep:b
#

jexec ${mjid} sysctl net.inet.ip.forwarding=1
jexec ${mjid} sysctl net.inet6.ip6.forwarding=1
jexec ${mjid} sysctl net.inet6.ip6.accept_rtadv=0

lmep=$(epair_base)
ifconfig ${lmep}a vnet ${ljid}
ifconfig ${lmep}b vnet ${mjid}

jexec ${ljid} ifconfig lo0 127.0.0.1/8
jexec ${ljid} ifconfig ${lmep}a inet  192.0.2.1/30 up
jexec ${ljid} ifconfig ${lmep}a inet6 2001:db8::1/64 alias

jexec ${ljid} route add default 192.0.2.2
jexec ${ljid} route add -inet6 default 2001:db8::2

jexec ${mjid} ifconfig lo0 127.0.0.1/8
jexec ${mjid} ifconfig ${lmep}b inet  192.0.2.2/30 up
jexec ${mjid} ifconfig ${lmep}b inet6 2001:db8::2/64 alias

mrep=$(epair_base)
ifconfig ${mrep}a vnet ${mjid}
ifconfig ${mrep}b vnet ${rjid}

jexec ${mjid} ifconfig ${mrep}a inet  192.0.2.5/30 up
jexec ${mjid} ifconfig ${mrep}a inet6 2001:db8:1::1/64 alias

jexec ${rjid} ifconfig lo0 127.0.0.1/8
jexec ${rjid} ifconfig ${mrep}b inet  192.0.2.6/30 up
jexec ${rjid} ifconfig ${mrep}b inet6 2001:db8:1::2/64 alias

jexec ${rjid} route add default 192.0.2.5
jexec ${rjid} route add -inet6 default 2001:db8:1::1

echo "press any key to continue (some do not work)"
read line

# Run some tests.
jexec ${ljid} ping -q -n -i 0.1 -c 3 192.0.2.1
jexec ${ljid} ping -q -n -i 0.1 -c 3 192.0.2.2
jexec ${ljid} ping -q -n -i 0.1 -c 3 192.0.2.5
jexec ${ljid} ping -q -n -i 0.1 -c 3 192.0.2.6

jexec ${ljid} ping6 -q -n -i 0.1 -c 3 2001:db8::1
jexec ${ljid} ping6 -q -n -i 0.1 -c 3 2001:db8::2
jexec ${ljid} ping6 -q -n -i 0.1 -c 3 2001:db8:1::1
jexec ${ljid} ping6 -q -n -i 0.1 -c 3 2001:db8:1::2

jexec ${ljid} traceroute -n 192.0.2.6
jexec ${ljid} traceroute6 -n 2001:db8:1::2

# Start netservers in both middle and right.
# Use different ports; netserver is a bit picky; else we'd need to sleep.
jexec ${mjid} netserver -p 20001 -4 -L 192.0.2.2
jexec ${mjid} netserver -p 20002 -6 -L 2001:db8::2
jexec ${rjid} netserver -p 20003 -4 -L 192.0.2.6
jexec ${rjid} netserver -p 20004 -6 -L 2001:db8:1::2

jexec ${ljid} netperf -p 20001 -H 192.0.2.2 -f m -L 192.0.2.1 -t TCP_STREAM
jexec ${ljid} netperf -p 20002 -H 2001:db8::2 -f m -L 2001:db8::1 -t TCP_STREAM
jexec ${ljid} netperf -p 20003 -H 192.0.2.6 -f m -L 192.0.2.1 -t TCP_STREAM
jexec ${ljid} netperf -p 20004 -H 2001:db8:1::2 -f m -L 2001:db8::1 -t TCP_STREAM

jexec ${ljid} netperf -p 20001 -H 192.0.2.2 -f m -L 192.0.2.1 -t UDP_STREAM
jexec ${ljid} netperf -p 20002 -H 2001:db8::2 -f m -L 2001:db8::1 -t UDP_STREAM
jexec ${ljid} netperf -p 20003 -H 192.0.2.6 -f m -L 192.0.2.1 -t UDP_STREAM
jexec ${ljid} netperf -p 20004 -H 2001:db8:1::2 -f m -L 2001:db8::1 -t UDP_STREAM

# Cleanup jails.
wait_enter
jail -r ${ljid}
wait_enter
jail -r ${mjid}
wait_enter
jail -r ${rjid}

ending

# end
