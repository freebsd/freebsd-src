#!/bin/sh
# $Id: extract_secure.sh,v 1.1 1995/01/14 07:41:45 jkh Exp $
PATH=/stand:$PATH
DDIR=/

DIST=secure
# You can't write over the running init
if [ -f /sbin/init ]; then mv /sbin/init /sbin/init.insecure; fi

echo "Extracting ${DIST}"
cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
