#!/bin/sh

set -x

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

#
# Helper(s).
#
epair_base()
{
	local ep
	
	ep=`ifconfig epair create`
	expr ${ep} : '\(.*\).'
}

#
# Setup "machines".
#

# Start left (client) jail.
ljid=`jail -i -c -n lef$$ host.hostname=pfvarn$$left.example.net vnet persist`

# Start middle (forwarding) jail.
mjid=`jail -i -c -n mid$$ host.hostname=pfvarn$$center.example.net vnet persist`

# Start right (server) jail.
rjid=`jail -i -c -n right$$ host.hostname=pfvarn$$right.example.net vnet persist`

echo "left ${ljid}   middle ${mjid}    right ${rjid}"

# Create networking.
#
# jail:         left            middle           right
# ifaces:       lmep:a ---- lmep:b  mrep:a ---- mrep:b
#

# IANA Benchmarking prefixes:
# 198.18.0.0/15
# 2001:0200::/48

jexec ${mjid} sysctl net.inet.ip.forwarding=1
jexec ${mjid} sysctl net.inet6.ip6.forwarding=1
jexec ${mjid} sysctl net.inet6.ip6.accept_rtadv=0

lmep=$(epair_base)
ifconfig ${lmep}a vnet ${ljid}
ifconfig ${lmep}b vnet ${mjid}

jexec ${ljid} ifconfig lo0 inet 127.0.0.1/8
jexec ${ljid} ifconfig ${lmep}a inet  198.18.0.1/30 up
jexec ${ljid} ifconfig ${lmep}a inet6 2001:200::1/64 alias

jexec ${ljid} route add default 198.18.0.2
jexec ${ljid} route add -inet6 default 2001:200::2

jexec ${mjid} ifconfig lo0 inet 127.0.0.1/8
jexec ${mjid} ifconfig ${lmep}b inet  198.18.0.2/30 up
jexec ${mjid} ifconfig ${lmep}b inet6 2001:200::2/64 alias

mrep=$(epair_base)
ifconfig ${mrep}a vnet ${mjid}
ifconfig ${mrep}b vnet ${rjid}

jexec ${mjid} ifconfig ${mrep}a inet  198.18.1.1/30 up
jexec ${mjid} ifconfig ${mrep}a inet6 2001:200:1::1/64 alias

jexec ${rjid} ifconfig lo0 inet 127.0.0.1/8
jexec ${rjid} ifconfig ${mrep}b inet  198.18.1.2/30 up
jexec ${rjid} ifconfig ${mrep}b inet6 2001:200:1::2/64 alias

jexec ${rjid} route add default 198.18.1.1
jexec ${rjid} route add -inet6 default 2001:200:1::1

jexec ${mjid} /sbin/pfctl -f  /home/test/conductor/tests/http/pf.conf
jexec ${mjid} /sbin/pfctl -e

jexec ${ljid} env thttpd_flags="-C /home/test/conductor/tests/http/thttpd.conf" /usr/local/etc/rc.d/thttpd onestart
jexec ${ljid} env varnishd_storage="file,/var/tmp,10M" varnishd_config=/usr/local/etc/varnish/example.vcl /usr/local/etc/rc.d/varnishd onestart

sleep 3

jexec ${rjid} /usr/local/bin/wrk -c 100 -d 3m -t 2 --timeout 10s http://198.18.0.1/

echo "TEARING DOWN JAILS IN 1s"
sleep 1

jail -r ${rjid} ${ljid} ${mjid}

# end
