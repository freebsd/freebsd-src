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

ending()
{

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
#id=`jail -i -c vnet persist`
#jail -r ${id}


# Private debugging.
# 0x01 == print; 0x02 destroy uma zones upon jail teardown no matter what
#sysctl security.jail.vimage_debug_memory=0
#sysctl security.jail.vimage_debug_memory=1
#sysctl security.jail.vimage_debug_memory=2
#sysctl security.jail.vimage_debug_memory=3

# Start jail.
jid=`jail -i -c host.hostname=left$$.example.net vnet persist`
echo "jid=${jid}"

# Generate a prefix ID from the process ID.
pid=$(printf "%x" `expr $$ % 65535`)

lmep=$(epair_base)
ifconfig ${lmep}a vnet ${jid}

jexec ${jid} ifconfig ${lmep}a inet6 2001:dbe8:dead:${pid}::1/64 up
ifconfig ${lmep}b inet6 2001:dbe8:dead:${pid}::2/64 up

ping6 -n -c 3 "2001:dbe8:dead:${pid}::1"
# Give ND/DUD a chance etc.
sleep 3
for i in `jot 254 1`; do
	ping6 -n -i 0.1 -c 1 "2001:dbe8:dead:${pid}::${i}"
done

# Cleanup jails.
wait_enter
jail -r ${jid}

ifconfig ${lmep}b destroy

ending

# end
