#!/bin/sh

# Move the dict stuff out to its own dist
if [ -d ${RD}/trees/bin/usr/share/dict ]; then
	tar -cf - -C ${RD}/trees/bin/usr/share/dict . |
		tar -xf - -C ${RD}/trees/dict/usr/share/dict &&
	rm -rf ${RD}/trees/bin/usr/share/dict;
fi

mkdir ${RD}/trees/dict/usr/share/misc

for i in airport birthtoken flowers inter.phone iso3166 na.phone zipcodes; do
	if [ -f ${RD}/trees/bin/usr/share/misc/$i ]; then
		mv ${RD}/trees/bin/usr/share/misc/$i
			${RD}/trees/dict/usr/share/misc;
	fi;
done
