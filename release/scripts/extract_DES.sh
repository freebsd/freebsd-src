#!/bin/sh
# $Id: extract_secure.sh,v 1.7 1995/04/20 06:49:09 phk Exp $
set -e
PATH=/stand:$PATH
DDIR=/

DIST=DES
# You can't write over the running init
if [ -f /sbin/init ]; then mv /sbin/init /sbin/init.insecure; fi

cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )

DDIR=/usr/src
for DIST in sebones sDES ; do
	if [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
