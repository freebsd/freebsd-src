#!/bin/sh
#
# $FreeBSD: src/release/scripts/proflibs-make.sh,v 1.8.24.1 2008/10/02 02:57:24 kensmith Exp $
#

# Move the profiled libraries out to their own dist
for i in ${RD}/trees/base/usr/lib/*_p.a; do
	mv $i ${RD}/trees/proflibs/usr/lib
done
