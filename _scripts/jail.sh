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

set -e

e1=`ifconfig epair create | sed -e 's/a$//'`
e2=`ifconfig epair create | sed -e 's/a$//'`
e3=`ifconfig epair create | sed -e 's/a$//'`

cleft=`jail  -i -c -n cleft$$  host.hostname=cleft$$.example.net  vnet persist`
rleft=`jail  -i -c -n rleft$$  host.hostname=rleft$$.example.net  vnet persist`
rright=`jail -i -c -n rright$$ host.hostname=rright$$.example.net vnet persist`
cright=`jail -i -c -n cright$$ host.hostname=cright$$.example.net vnet persist`

for jid in ${cleft} ${rleft} ${rright} ${cright}; do
	jexec ${jid} ifconfig lo0 inet  127.0.0.1/8 alias
	jexec ${jid} ifconfig lo0 inet6 ::1/128 alias
done

ifconfig ${e1}a vnet ${cleft}
ifconfig ${e1}b vnet ${rleft}
jexec ${cleft}  ifconfig ${e1}a inet6 2001:db8:6c::1
jexec ${rleft}  ifconfig ${e1}b inet6 2001:db8:6c::2

ifconfig ${e2}a vnet ${rleft}
ifconfig ${e2}b vnet ${rright}
jexec ${rleft}  ifconfig ${e2}a inet6 2001:db8:6d::1
jexec ${rright} ifconfig ${e2}b inet6 2001:db8:6d::2

ifconfig ${e3}a vnet ${rright}
ifconfig ${e3}b vnet ${cright}
jexec ${rright} ifconfig ${e3}a inet6 2001:db8:72::1
jexec ${cright} ifconfig ${e3}b inet6 2001:db8:72::2

jexec ${rleft}  sysctl net.inet.ip.forwarding=1
jexec ${rleft}  sysctl net.inet6.ip6.forwarding=1
jexec ${rright} sysctl net.inet.ip.forwarding=1
jexec ${rright} sysctl net.inet6.ip6.forwarding=1

jexec ${cleft}  route add -inet6 default 2001:db8:6c::2
jexec ${rleft}  route add -inet6 default 2001:db8:6d::2
jexec ${rright} route add -inet6 default 2001:db8:6d::1
jexec ${cright} route add -inet6 default 2001:db8:72::1


# end
