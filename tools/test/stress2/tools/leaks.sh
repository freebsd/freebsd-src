#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Script to catch possible leaks in vm, malloc and mbufs
# Report values growing in 10 consecuitive samples

i=0
while true; do
   # Check for leaks in vm.zone

# ITEM            SIZE     LIMIT     USED    FREE  REQUESTS
#
# UMA Kegs:        140,        0,      66,      6,       66
# UMA Zones:       120,        0,      66,     24,       66

   vmstat -z | sed '1,3d;s/://g' | \
   sed 's/  */ /g;s/\([0-9]\)  *\([0-9]\)/\1,\2/g;s/ \([0-9]\)/,\1/;s/^ *//' | \
   awk -F, '
/^..*$/{
	gsub("^ *", "", $1);
	size=$4;
	printf "vmstat -z %s,%s\n", $1, size;
}
'
   # vmstat -m

   #          Type InUse MemUse HighUse Requests  Size(s)
   #        DEVFS3   168    21K       -      169  128
   #        DEVFS1   157    40K       -      157  256
   #         DEVFS    12     1K       -       13  16,128
	vmstat -m | \
	sed '1,1d;s/K .*//;s/ [0-9][0-9]* / /;s/  */ /g;s/^ *//;s/ /_/g;s/_\([0-9][0-9]*$\)/ \1/' | \
	awk '{printf "vmstat -m %s, %d\n", $1, $2}'

   # Check for leaks in mbufs

# $ netstat -m
# 1233/597/1830 mbufs in use (current/cache/total)
# 1232/196/1428/8896 mbuf clusters in use (current/cache/total/max)
# 1232/74 mbuf+clusters out of packet secondary zone in use (current/cache)
# 0/0/0/0 4k (page size) jumbo clusters in use (current/cache/total/max)
# 0/0/0/0 9k jumbo clusters in use (current/cache/total/max)
# 0/0/0/0 16k jumbo clusters in use (current/cache/total/max)
# 2772K/541K/3313K bytes allocated to network (current/cache/total)
# 508/7778/5734 requests for mbufs denied (mbufs/clusters/mbuf+clusters)
# 0/0/0 requests for jumbo clusters denied (4k/9k/16k)
# 0/6/2480 sfbufs in use (current/peak/max)
# 0 requests for sfbufs denied
# 0 requests for sfbufs delayed
# 0 requests for I/O initiated by sendfile
# 251 calls to protocol drain routines

   netstat -m | head -10 | sed 's#/# #g;s/k / /;s/K / /' | awk '
/mbufs /     {mbufs=$1};
/ clusters/  {clusters=$2};
/sfbufs in use/    {sfbufs=$3};
/allocated/ {allocated=$1}
END {
	print "mbufs,", mbufs;
	print "clusters,", clusters;
	print "sfbufs,", sfbufs;
	print "allocatedToNetwork,", allocated;
}
'
   sysctl vm.kvm_free | tail -1 | sed 's/:/,/'
   sysctl vm.swap_reserved | tail -1 | sed 's/:/,/'
   sleep 10
done | awk -F, '
{
# Pairs of "name, value" are passed to this awk script
	name=$1;
	size=$2;
#	print "name, size :", name, size;
	if (NF != 2)
		print "Number of fields for ", name, "is ", NF;
	if (size > s[name]) {
		n[name]++;
		if (n[name] > 50) {
			cmd="date '+%T'";
			cmd | getline t;
			close(cmd);
			printf "%s \"%s\" may be leaking, used %d\r\n", t, name, size;
			n[name] = 0;
		}
		s[name] = size;
	}
}'
