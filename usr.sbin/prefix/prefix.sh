#!/bin/sh

#	$KAME: prefix.sh,v 1.12 2001/05/26 23:38:10 itojun Exp $
#	$FreeBSD$

# Copyright (c) 2001 WIDE Project. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the project nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

iface=$1
prefix=$2

usage() {
    echo "usage: prefix interface prefix [set|delete]"
}

# We're now invalidating the prefix ioctls and the corresponding command.
echo "** The prefix command is almost invalidated. Please use ifconfig(8). **"

if [ X"$iface" = X -o X"$prefix" = X ]; then
    usage
    exit 1
fi

if [ -z $3 ]; then
    command=set
else
    command=$3
fi

case $command in
    set)
	laddr=`ifconfig $iface inet6 | grep 'inet6 fe80:' | head -1 | awk '{print $2}'` 
	if [ X"$laddr" = X ]; then
	    echo "prefix: no interface ID found"
	    exit 1
	fi
	hostid=`echo $laddr | sed -e 's/^fe80:[0-9a-fA-F]*:/fe80::/' -e 's/^fe80:://' -e 's/%.*//'`
	address=$2$hostid
	exec ifconfig $iface inet6 $address prefixlen 64 alias
    ;;
    delete)
    	addrs=`ifconfig $iface inet6 | grep "inet6 $prefix" |  awk '{print $2}'`
	for a in $addrs; do
	    ifconfig $iface inet6 $a -alias
	done
    ;;
    *)
	usage
	exit 1
    ;;
esac
