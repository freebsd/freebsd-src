#!/bin/sh

# Create the doc dist.
if [ -d ${RD}/trees/bin/usr/share/doc ]; then
	( cd ${RD}/trees/bin/usr/share/doc;
	find . | cpio -dumpl ${RD}/trees/doc/usr/share/doc ) &&
	rm -rf ${RD}/trees/bin/usr/share/doc
fi
