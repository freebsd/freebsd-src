#!/bin/sh
# $Id: extract_compat1x.sh,v 1.1 1995/01/14 07:41:40 jkh Exp $
set -e
PATH=/stand:$PATH
DDIR=/

DIST=compat1x_tgz
echo "Extracting ${DIST}"
cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
