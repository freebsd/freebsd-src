#!/bin/sh

# $FreeBSD$

# Move the info files out to their own dist
if [ -d ${RD}/trees/bin/usr/share/info ]; then
	tar -cf - -C ${RD}/trees/bin/usr/share/info . |
		tar -xpf - -C ${RD}/trees/info/usr/share/info &&
	rm -rf ${RD}/trees/bin/usr/share/info;
fi
