#!/bin/sh
#
# $FreeBSD: src/release/scripts/proflibs-make.sh,v 1.8.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#

# Move the profiled libraries out to their own dist
for i in ${RD}/trees/base/usr/lib/*_p.a; do
	mv $i ${RD}/trees/proflibs/usr/lib
done
