#!/bin/sh
#
# $FreeBSD: src/release/scripts/proflibs-make.sh,v 1.7 2002/04/23 22:16:40 obrien Exp $
#

# Move the profiled libraries out to their own dist
for i in ${RD}/trees/base/usr/lib/*_p.a; do
	if [ -f $i ]; then
		mv $i ${RD}/trees/proflibs/usr/lib;
	fi;
done
