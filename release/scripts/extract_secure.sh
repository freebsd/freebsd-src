#!/bin/sh
# $Id: extract_secure.sh,v 1.6 1995/04/07 01:20:26 phk Exp $
set -e
PATH=/stand:$PATH
DDIR=/

DIST=secure
# You can't write over the running init
if [ -f /sbin/init ]; then mv /sbin/init /sbin/init.insecure; fi

cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )

DDIR=/usr/src
for DIST in sebones ssecure ; do
	if [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
