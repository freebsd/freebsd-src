#!/bin/sh
# $Id: extract_secure.sh,v 1.4 1995/01/28 09:11:39 jkh Exp $
PATH=/stand:$PATH
DDIR=/

DIST=secure
# You can't write over the running init
if [ -f /sbin/init ]; then mv /sbin/init /sbin/init.insecure; fi

cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )

DDIR=/usr/src
for DIST in ebones secrsrc; do
	if [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
