#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Memory leak detector: run vmstat -m & -z in a loop.

export LANG=en_US.ISO8859-1
while getopts dmz flag; do
        case "$flag" in
        d) debug="-v debug=1" ;;
        m) optz=n ;;
        z) optm=n ;;
        *) echo "Usage $0 [-d] [-m] [-z]"
           return 1 ;;
        esac
done

pages=`sysctl -n vm.stats.vm.v_page_count`
start=`date '+%s'`
OIFS=$IFS
while true; do
	#          Type InUse MemUse
	[ -z "$optm" ] && vmstat -m | sed 1d |
	    while read l; do
		name=`echo $l | sed -E 's/ [0-9]+ .*//; s/^ *//'`
		memuse=`echo $l | sed -E "s#$name##" | \
		    awk '{print int(($2 + 1023) / 1024)}'`
		[ "$memuse" -ne 0 ] && echo "vmstat -m $name,$memuse"
	    done

	# ITEM                   SIZE  LIMIT     USED
	[ -z "$optz" ] && vmstat -z | sed 1d |
	    while read l; do
		name=`echo $l | sed 's/:.*//'`
		l=`echo $l | sed 's/.*://'`
		size=`echo $l | awk -F ',' '{print $1}'`
		used=`echo $l | awk -F ',' '{print $3}'`
		[ -z "$used" -o -z "$size" ] &&
		    { echo "used/size not set $l" 1>&2; continue; }
		echo $used | egrep -q '^ *[0-9]{1,10}$' ||
		    { echo "Bad used: $used. l=$l" 1>&2; continue; }
		tot=$((((size * used) + 1023) / 1024))
		[ $tot -ne 0 ] &&
		   echo "vmstat -z $name,$tot"
	    done

	r=`sysctl -n vm.stats.vm.v_wire_count`
	[ -n "$r" ] &&
	echo "vm.stats.vm.v_wire_count, \
	    $((r * 4))"
	r=`sysctl -n vm.stats.vm.v_free_count`
	[ -n "$r" ] &&
	echo "pages in use, \
	    $(((pages - r) * 4))"
	r=`sysctl -n vm.kmem_map_size`
	[ -n "$r" ] &&
	echo "kmem_map_size, $r"
	sleep 10
done | awk $debug -F, '
{
# Pairs of "name, value" are passed to this awk script.
	name=$1;
	size=$2;
	if (size > s[name]) {
		if (++n[name] > 60) {
			cmd="date '+%T'";
			cmd | getline t;
			close(cmd);
			printf "%s \"%s\" %'\''dK\r\n", t,
			    name, size;
			fflush
			n[name] = 0;
		}
		s[name] = size;
		if (debug == 1 && n[name] > 1)
			printf "%s, size %d, count %d\r\n",
			    name, s[name], n[name] > "/dev/stderr"
	} else if (size < s[name] && n[name] > 0)
		n[name]--
}' | while read l; do
	d=$(((`date '+%s'` - start) / 86400))
	echo "$d $l"
done
# Note: the %'d is used to trigger a thousands-separator character.
