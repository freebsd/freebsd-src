#!/bin/sh
#
# $FreeBSD$
#

# Move all the games out to their own dist
if [ -d ${RD}/trees/base/usr/games ]; then
	tar -cf - -C ${RD}/trees/base/usr/games . |
		tar -xpf - -C ${RD}/trees/games/usr/games &&
	rm -rf ${RD}/trees/base/usr/games;
fi

if [ -d ${RD}/trees/base/usr/share/games ]; then
	tar -cf - -C ${RD}/trees/base/usr/share/games . |
		tar -xpf - -C ${RD}/trees/games/usr/share/games &&
	rm -rf ${RD}/trees/base/usr/share/games;
fi

if [ -d ${RD}/trees/base/var/games ]; then
	tar -cf - -C ${RD}/trees/base/var/games . |
		tar -xpf - -C ${RD}/trees/games/var/games &&
	rm -rf ${RD}/trees/base/var/games;
fi

if [ -d ${RD}/trees/manpages/usr/share/man/man6 ]; then
	mkdir -p ${RD}/trees/games/usr/share/man/man6
	tar -cf - -C ${RD}/trees/manpages/usr/share/man/man6 . |
		tar -xpf - -C ${RD}/trees/games/usr/share/man/man6 &&
	rm -rf ${RD}/trees/manpages/usr/share/man/man6
fi

if [ -d ${RD}/trees/catpages/usr/share/man/cat6 ]; then
	mkdir -p ${RD}/trees/games/usr/share/man/cat6
	tar -cf - -C ${RD}/trees/catpages/usr/share/man/cat6 . |
		tar -xpf - -C ${RD}/trees/games/usr/share/man/cat6 &&
	rm -rf ${RD}/trees/catpages/usr/share/man/cat6
fi
