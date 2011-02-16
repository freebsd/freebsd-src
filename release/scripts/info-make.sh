#!/bin/sh
#
# $FreeBSD: src/release/scripts/info-make.sh,v 1.4.40.1 2010/12/21 17:10:29 kensmith Exp $
#

# Move the info files out to their own dist
if [ -d ${RD}/trees/base/usr/share/info ]; then
	tar -cf - -C ${RD}/trees/base/usr/share/info . |
		tar -xpf - -C ${RD}/trees/info/usr/share/info &&
	rm -rf ${RD}/trees/base/usr/share/info;
fi
