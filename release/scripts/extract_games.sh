#!/bin/sh
# $Id: extract.sh,v 1.17 1994/12/04 03:41:18 jkh Exp $
PATH=/stand:$PATH
DDIR=/

DIST=games
echo "Extracting ${DIST}"
cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
