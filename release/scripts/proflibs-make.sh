#!/bin/sh
#
# $FreeBSD: src/release/scripts/proflibs-make.sh,v 1.5.6.1 2002/08/08 08:23:53 ru Exp $
#

# Move the profiled libraries out to their own dist
for i in ${RD}/trees/bin/usr/lib/*_p.a; do
	if [ -f $i ]; then
		mv $i ${RD}/trees/proflibs/usr/lib;
	fi;
done
