#!/bin/sh

# $FreeBSD: src/release/scripts/dict-make.sh,v 1.7 1999/09/11 06:11:52 jkh Exp $

# Move the dict stuff out to its own dist
if [ -d ${RD}/trees/bin/usr/share/dict ]; then
	tar -cf - -C ${RD}/trees/bin/usr/share/dict . |
		tar -xpf - -C ${RD}/trees/dict/usr/share/dict &&
	rm -rf ${RD}/trees/bin/usr/share/dict;
fi

for i in airport birthtoken flowers inter.phone na.phone zipcodes; do
	if [ -f ${RD}/trees/bin/usr/share/misc/$i ]; then
		mv ${RD}/trees/bin/usr/share/misc/$i \
			${RD}/trees/dict/usr/share/misc;
	fi;
done
