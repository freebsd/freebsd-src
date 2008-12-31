#!/bin/sh
#
# $FreeBSD: src/release/scripts/manpages-make.sh,v 1.5.32.1 2008/11/25 02:59:29 kensmith Exp $
#

# Move all the manpages out to their own dist, using the base dist as a
# starting point.
if [ -d ${RD}/trees/base/usr/share/man ]; then
	( cd ${RD}/trees/base/usr/share/man;
	find . | cpio -dumpl ${RD}/trees/manpages/usr/share/man > /dev/null 2>&1) &&
	rm -rf ${RD}/trees/base/usr/share/man;
fi
