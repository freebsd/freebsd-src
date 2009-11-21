#!/bin/sh
#
# $FreeBSD: src/release/scripts/doc-make.sh,v 1.3.36.1.2.1 2009/10/25 01:10:29 kensmith Exp $
#

# Create the doc dist.
if [ -d ${RD}/trees/base/usr/share/doc ]; then
	( cd ${RD}/trees/base/usr/share/doc;
	find . | cpio -dumpl ${RD}/trees/doc/usr/share/doc ) &&
	rm -rf ${RD}/trees/base/usr/share/doc
fi
