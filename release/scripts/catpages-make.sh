#!/bin/sh
#
# $FreeBSD$
#

# Move all the catpages out to their own dist, using the bin dist as a
# starting point.  This must precede the manpages dist script.
if [ -d ${RD}/trees/bin/usr/share/man ]; then
	( cd ${RD}/trees/bin/usr/share/man;
	find cat* whatis | cpio -dumpl ${RD}/trees/catpages/usr/share/man > /dev/null 2>&1) &&
	rm -rf ${RD}/trees/bin/usr/share/man/cat*;
fi
if [ -d ${RD}/trees/bin/usr/share/perl/man ]; then
	( cd ${RD}/trees/bin/usr/share/perl/man;
	find cat* whatis | cpio -dumpl ${RD}/trees/catpages/usr/share/perl/man > /dev/null 2>&1) &&
	rm -rf ${RD}/trees/bin/usr/share/perl/man/cat*;
fi
