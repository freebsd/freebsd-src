#!/bin/sh
# $Id: extract_proflibs.sh,v 1.1 1995/01/14 07:41:44 jkh Exp $
PATH=/stand:$PATH
DDIR=/

DIST=proflibs
echo "Extracting ${DIST} - ignore any errors from cpio"
cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
