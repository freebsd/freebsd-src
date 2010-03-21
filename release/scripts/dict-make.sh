#!/bin/sh
#
# $FreeBSD: src/release/scripts/dict-make.sh,v 1.10.38.1 2010/02/10 00:26:20 kensmith Exp $
#

# Move the dict stuff out to its own dist
if [ -d ${RD}/trees/base/usr/share/dict ]; then
	tar -cf - -C ${RD}/trees/base/usr/share/dict . |
		tar -xpf - -C ${RD}/trees/dict/usr/share/dict &&
	rm -rf ${RD}/trees/base/usr/share/dict;
fi

for i in birthtoken flowers; do
	if [ -f ${RD}/trees/base/usr/share/misc/$i ]; then
		mv ${RD}/trees/base/usr/share/misc/$i \
			${RD}/trees/dict/usr/share/misc;
	fi;
done
