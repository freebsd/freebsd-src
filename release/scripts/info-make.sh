#!/bin/sh
#
# $FreeBSD: src/release/scripts/info-make.sh,v 1.2.2.1 2002/08/08 08:23:53 ru Exp $
#

# Move the info files out to their own dist
if [ -d ${RD}/trees/bin/usr/share/info ]; then
	tar -cf - -C ${RD}/trees/bin/usr/share/info . |
		tar -xpf - -C ${RD}/trees/info/usr/share/info &&
	rm -rf ${RD}/trees/bin/usr/share/info;
fi
