#!/bin/sh
# $Id: extract_des.sh,v 1.1 1995/05/09 22:58:42 jkh Exp $
set -e
PATH=/stand:$PATH
DDIR=/

DIST=des
# You can't write over the running init
if [ -f /sbin/init ]; then mv /sbin/init /sbin/init.insecure; fi

cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )

DDIR=/usr/src
for DIST in sebones sdes ; do
	if [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
