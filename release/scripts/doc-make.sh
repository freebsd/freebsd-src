#!/bin/sh
#
# $FreeBSD: src/release/scripts/doc-make.sh,v 1.3.30.1 2008/10/02 02:57:24 kensmith Exp $
#

# Create the doc dist.
if [ -d ${RD}/trees/base/usr/share/doc ]; then
	( cd ${RD}/trees/base/usr/share/doc;
	find . | cpio -dumpl ${RD}/trees/doc/usr/share/doc ) &&
	rm -rf ${RD}/trees/base/usr/share/doc
fi
