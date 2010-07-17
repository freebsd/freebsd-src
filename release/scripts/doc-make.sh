#!/bin/sh
#
# $FreeBSD: src/release/scripts/doc-make.sh,v 1.3.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#

# Create the doc dist.
if [ -d ${RD}/trees/base/usr/share/doc ]; then
	( cd ${RD}/trees/base/usr/share/doc;
	find . | cpio -dumpl ${RD}/trees/doc/usr/share/doc ) &&
	rm -rf ${RD}/trees/base/usr/share/doc
fi
