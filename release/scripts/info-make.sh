#!/bin/sh

# $FreeBSD: src/release/scripts/info-make.sh,v 1.2 1999/09/11 06:11:52 jkh Exp $

# Move the info files out to their own dist
if [ -d ${RD}/trees/bin/usr/share/info ]; then
	tar -cf - -C ${RD}/trees/bin/usr/share/info . |
		tar -xpf - -C ${RD}/trees/info/usr/share/info &&
	rm -rf ${RD}/trees/bin/usr/share/info;
fi
