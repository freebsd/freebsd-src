#!/bin/sh

# Move all the games out to their own dist
if [ -d ${RD}/trees/bin/usr/games ]; then
	tar -cf - -C ${RD}/trees/bin/usr/games . |
		tar -xf - -C ${RD}/trees/games/usr/games &&
	rm -rf ${RD}/trees/bin/usr/games;
fi

if [ -d ${RD}/trees/bin/usr/share/games ]; then
	tar -cf - -C ${RD}/trees/bin/usr/share/games . |
		tar -xf - -C ${RD}/trees/games/usr/share/games &&
	rm -rf ${RD}/trees/bin/usr/share/games;
fi

if [ -d ${RD}/trees/bin/var/games ]; then
	tar -cf - -C ${RD}/trees/bin/var/games . |
		tar -xf - -C ${RD}/trees/games/var/games &&
	rm -rf ${RD}/trees/bin/var/games;
fi
