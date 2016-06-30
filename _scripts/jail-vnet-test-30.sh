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

pid=$$

# Start jail.
jid=`jail -i -c host.hostname=${pid}.example.net vnet persist`
echo "jid=${jid}"

# Generate a prefix ID from the process ID.
pid=$(printf "%d" `expr ${pid} % 65535`)

# Create a disc interface and move it into the jail.
ifconfig disc${pid} create
ifconfig disc${pid} vnet ${jid}

# Create another disc interface and rename it to the same name
# as the one sitting in the vnet.
pidn=$((pid + 1))
ifconfig disc${pidn} create
ifconfig disc${pidn} name disc${pid}

ifconfig -l
wait_enter

# Now we could just teardown the jail but to ease debugging
# let it alive and just move the interface back to base.
ifconfig disc${pid} -vnet ${jid}
# ^^^^^^^^^^^^ this is the problem and we will "lose" the iface.

ifconfig -l
ifconfig -a | grep disc${pid}
ifconfig -a | grep disc${pidn}
# XXX once we no longer lose the iface, clean it up here as well.

# Now cleanup jails.
wait_enter
jail -r ${jid}

ifconfig disc${pid} destroy
ifconfig disc${pid} destroy

ifconfig -l

ending

# end
