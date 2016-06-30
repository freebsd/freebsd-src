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

# Private debugging.
# 0x01 == print; 0x02 destroy uma zones upon jail teardown no matter what
#sysctl security.jail.vimage_debug_memory=0
#sysctl security.jail.vimage_debug_memory=1
#sysctl security.jail.vimage_debug_memory=2
#sysctl security.jail.vimage_debug_memory=3

#vmstat -z > ${prefix}-vmstat-z-1
#vmstat -m > ${prefix}-vmstat-m-1

# Start left (client) jail.
ljid=`jail -i -c -n lef$$ host.hostname=left$$.example.net vnet persist`

# Start middle (forwarding) jail.
mjid=`jail -i -c -n mid$$ host.hostname=center$$.example.net vnet persist`

# Start right (server) jail.
rjid=`jail -i -c -n right$$ host.hostname=right$$.example.net vnet persist`

echo "left ${ljid}   middle ${mjid}    right ${rjid}"

# Create networking.
#
# jail:		left            middle           right
# ifaces:	lmep:a ---- lmep:b  mrep:a ---- mrep:b
#

jexec ${mjid} sysctl net.inet6.ip6.forwarding=1
jexec ${mjid} sysctl net.inet6.ip6.accept_rtadv=0

lmep=$(epair_base)
ifconfig ${lmep}a vnet ${ljid}
ifconfig ${lmep}b vnet ${mjid}

jexec ${ljid} ifconfig ${lmep}a inet6 2001:db8::1/64 alias

jexec ${ljid} route add -inet6 default 2001:db8::2

jexec ${mjid} ifconfig ${lmep}b inet6 2001:db8::2/64 alias

mrep=$(epair_base)
ifconfig ${mrep}a vnet ${mjid}
ifconfig ${mrep}b vnet ${rjid}

jexec ${mjid} ifconfig ${mrep}a inet6 2001:db8:1::1/64 alias

jexec ${rjid} ifconfig ${mrep}b inet6 2001:db8:1::2/64 alias

jexec ${rjid} route add -inet6 default 2001:db8:1::1

ending

# end
