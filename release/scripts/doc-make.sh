#!/bin/sh
#
# $FreeBSD: src/release/scripts/doc-make.sh,v 1.3.32.1 2008/11/25 02:59:29 kensmith Exp $
#

# Create the doc dist.
if [ -d ${RD}/trees/base/usr/share/doc ]; then
	( cd ${RD}/trees/base/usr/share/doc;
	find . | cpio -dumpl ${RD}/trees/doc/usr/share/doc ) &&
	rm -rf ${RD}/trees/base/usr/share/doc
fi
