#!/bin/sh

# Move the info files out to their own dist
if [ -d ${RD}/trees/bin/usr/share/info ]; then
	tar -cf - -C ${RD}/trees/bin/usr/share/info . |
		tar -xf - -C ${RD}/trees/info/usr/share/info &&
	rm -rf ${RD}/trees/bin/usr/share/info;
fi
