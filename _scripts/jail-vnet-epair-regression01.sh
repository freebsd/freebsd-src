#!/bin/sh
#-
# Copyright (c) 2011, "Bjoern A. Zeeb" <bz@FreeBSD.org>
# All rights reserved.
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
# $FreeBS$
#

# Based on a report:
# ------------------------------------------------
# Date: Thu, 20 Jan 2011 22:47:02 +0200
# From: Mikolaj Golub <to.my.trociny@gmail.com>
# To: freebsd-virtualization@FreeBSD.org
# Message-ID: <86k4hz5mm1.fsf@kopusha.home.net>

PID=$$

IF_base()
{
	local _if ifname

	ifname=$1
	
	_if=`ifconfig ${ifname} create`
	expr ${_if} : '\(.*\).'
}

echo "==> Running regression01.."
jid=`jail -i -c -n regression01-${PID} host.hostname=regression01-$$.example.org vnet persist`
ep=$(IF_base epair)
ifconfig ${ep} create
ifconfig ${ep}b vnet ${jid}
ifconfig ${ep}a destroy

# This would panic.
jexec ${jid} ifconfig -an

echo "Cleaning up."
jail -r ${jid}

# end
