#!/bin/sh
#
# $FreeBSD: src/release/scripts/catpages-make.sh,v 1.7.30.1 2008/10/02 02:57:24 kensmith Exp $
#

# Move all the catpages out to their own dist, using the base dist as a
# starting point.  This must precede the manpages dist script.
if [ -d ${RD}/trees/base/usr/share/man ]; then
	( cd ${RD}/trees/base/usr/share/man;
	find cat* whatis | cpio -dumpl ${RD}/trees/catpages/usr/share/man > /dev/null 2>&1) &&
	rm -rf ${RD}/trees/base/usr/share/man/cat*;
fi
