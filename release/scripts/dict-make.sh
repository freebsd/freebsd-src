#!/bin/sh

# $FreeBSD$

# Move the dict stuff out to its own dist
if [ -d ${RD}/trees/bin/usr/share/dict ]; then
	tar -cf - -C ${RD}/trees/bin/usr/share/dict . |
		tar -xpf - -C ${RD}/trees/dict/usr/share/dict &&
	rm -rf ${RD}/trees/bin/usr/share/dict;
fi

for i in birthtoken flowers; do
	if [ -f ${RD}/trees/bin/usr/share/misc/$i ]; then
		mv ${RD}/trees/bin/usr/share/misc/$i \
			${RD}/trees/dict/usr/share/misc;
	fi;
done
