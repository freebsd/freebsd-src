#!/bin/sh
# $Id: extract_manpages.sh,v 1.1 1995/01/14 07:41:43 jkh Exp $
set -e
PATH=/stand:$PATH
DDIR=/

DIST=manpages
echo "Extracting ${DIST}"
cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
