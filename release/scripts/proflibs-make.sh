#!/bin/sh
#
# $FreeBSD$
#

# Move the profiled libraries out to their own dist
for i in ${RD}/trees/bin/usr/lib/*_p.a; do
	if [ -f $i ]; then
		mv $i ${RD}/trees/proflibs/usr/lib;
	fi;
done
