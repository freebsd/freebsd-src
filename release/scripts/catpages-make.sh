#!/bin/sh

# Create the catpages dist - must follow manpages dist script, for obvious
# reasons.
if [ -d ${RD}/trees/manpages/usr/share/man ]; then
	su -m man -c 'catman ${RD}/trees/manpages/usr/share/man' > /dev/null 2>&1;
	( cd ${RD}/trees/manpages/usr/share/man;
	find cat* | cpio -dumpl ${RD}/trees/catpages/usr/share/man ) &&
	rm -rf ${RD}/trees/manpages/usr/share/man/cat*;
fi
