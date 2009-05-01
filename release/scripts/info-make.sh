#!/bin/sh
#
# $FreeBSD: src/release/scripts/info-make.sh,v 1.4.34.1 2009/04/15 03:14:26 kensmith Exp $
#

# Move the info files out to their own dist
if [ -d ${RD}/trees/base/usr/share/info ]; then
	tar -cf - -C ${RD}/trees/base/usr/share/info . |
		tar -xpf - -C ${RD}/trees/info/usr/share/info &&
	rm -rf ${RD}/trees/base/usr/share/info;
fi
