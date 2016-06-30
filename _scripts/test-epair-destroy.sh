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

# Start a jail
ajid=`jail -i -c -n ajail$$ host.hostname=ajail$$.example.net vnet persist`

lmep=$(epair_base)
ifconfig ${lmep}a vnet ${ajid}

jexec ${ajid} ifconfig ${lmep}a inet6 2001:db8::1/64 alias

ifconfig ${lmep}b inet6 2001:db8::2/64 alias

ifconfig ${lmep}b destroy

ending

# end
