#!/usr/bin/ksh
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
#

#
# Test {udp,ip}:::{send,receive} of IPv4 UDP to a remote host.
#
# This may fail due to:
#
# 1. A change to the ip stack breaking expected probe behavior,
#    which is the reason we are testing.
# 2. No physical network interface is plumbed and up.
# 3. No other hosts on this subnet are reachable and listening on rpcbind.
# 4. An unlikely race causes the unlocked global send/receive
#    variables to be corrupted.
#
# This test sends a UDP message using ping and checks that at least the
# following counts were traced:
#
# 1 x ip:::send (UDP sent to ping's base UDP port)
# 1 x udp:::send (UDP sent to ping's base UDP port)
# 

if (( $# != 1 )); then
	print -u2 "expected one argument: <dtrace-path>"
	exit 2
fi

dtrace=$1
getaddr=./get.ipv4remote.pl

if [[ ! -x $getaddr ]]; then
	print -u2 "could not find or execute sub program: $getaddr"
	exit 3
fi
$getaddr | read source dest
if (( $? != 0 )); then
	exit 4
fi

$dtrace -c "/sbin/ping -U $dest" -qs /dev/stdin <<EOF | grep -v 'is alive'
BEGIN
{
	ipsend = udpsend = 0;
}

ip:::send
/args[2]->ip_saddr == "$source" && args[2]->ip_daddr == "$dest" &&
    args[4]->ipv4_protocol == IPPROTO_UDP/
{
	ipsend++;
}

udp:::send
/args[2]->ip_saddr == "$source" && args[2]->ip_daddr == "$dest"/
{
	udpsend++;
}

END
{
	printf("Minimum UDP events seen\n\n");
	printf("ip:::send - %s\n", ipsend >= 1 ? "yes" : "no");
	printf("udp:::send - %s\n", udpsend >= 1 ? "yes" : "no");
}
EOF
