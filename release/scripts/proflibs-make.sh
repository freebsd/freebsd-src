#!/bin/sh
#
# $FreeBSD: src/release/scripts/proflibs-make.sh,v 1.8 2004/01/18 09:06:40 ru Exp $
#

# Move the profiled libraries out to their own dist
for i in ${RD}/trees/base/usr/lib/*_p.a; do
	mv $i ${RD}/trees/proflibs/usr/lib
done
