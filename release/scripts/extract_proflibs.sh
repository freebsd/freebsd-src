#!/bin/sh
# $Id: extract_proflibs.sh,v 1.2 1995/01/28 09:11:36 jkh Exp $
set -e
PATH=/stand:$PATH
DDIR=/

DIST=proflibs
echo "Extracting ${DIST} - ignore any errors from cpio"
cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
