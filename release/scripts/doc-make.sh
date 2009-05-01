#!/bin/sh
#
# $FreeBSD: src/release/scripts/doc-make.sh,v 1.3.34.1 2009/04/15 03:14:26 kensmith Exp $
#

# Create the doc dist.
if [ -d ${RD}/trees/base/usr/share/doc ]; then
	( cd ${RD}/trees/base/usr/share/doc;
	find . | cpio -dumpl ${RD}/trees/doc/usr/share/doc ) &&
	rm -rf ${RD}/trees/base/usr/share/doc
fi
