#!/bin/sh
#
# $FreeBSD: src/release/scripts/info-make.sh,v 1.4 2002/04/23 22:16:40 obrien Exp $
#

# Move the info files out to their own dist
if [ -d ${RD}/trees/base/usr/share/info ]; then
	tar -cf - -C ${RD}/trees/base/usr/share/info . |
		tar -xpf - -C ${RD}/trees/info/usr/share/info &&
	rm -rf ${RD}/trees/base/usr/share/info;
fi
