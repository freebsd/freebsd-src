#!/bin/sh
# $Id: extract_secure.sh,v 1.2 1995/01/14 07:44:23 jkh Exp $
PATH=/stand:$PATH
DDIR=/

DIST=secure
# You can't write over the running init
if [ -f /sbin/init ]; then mv /sbin/init /sbin/init.insecure; fi

echo "Extracting ${DIST}"
cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )

for DIST in ebones secrsrc; do
	if [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
