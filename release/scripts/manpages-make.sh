#!/bin/sh
#
# $FreeBSD: src/release/scripts/manpages-make.sh,v 1.1.8.1 2002/07/25 09:33:17 ru Exp $
#

# Move all the manpages out to their own dist, using the bin dist as a
# starting point.
if [ -d ${RD}/trees/bin/usr/share/man ]; then
	( cd ${RD}/trees/bin/usr/share/man;
	find . | cpio -dumpl ${RD}/trees/manpages/usr/share/man > /dev/null 2>&1) &&
	rm -rf ${RD}/trees/bin/usr/share/man;
fi
if [ -d ${RD}/trees/bin/usr/share/perl/man ]; then
	( cd ${RD}/trees/bin/usr/share/perl/man;
	find . | cpio -dumpl ${RD}/trees/manpages/usr/share/perl/man > /dev/null 2>&1) &&
	rm -rf ${RD}/trees/bin/usr/share/perl/man;
fi
