#!/bin/sh
#
# $FreeBSD: src/release/scripts/doc-make.sh,v 1.1.10.1 2002/08/08 08:23:53 ru Exp $
#

# Create the doc dist.
if [ -d ${RD}/trees/bin/usr/share/doc ]; then
	( cd ${RD}/trees/bin/usr/share/doc;
	find . | cpio -dumpl ${RD}/trees/doc/usr/share/doc ) &&
	rm -rf ${RD}/trees/bin/usr/share/doc
fi
