#!/bin/sh
# $Id: extract_dict.sh,v 1.1 1995/01/14 07:41:41 jkh Exp $
set -e
PATH=/stand:$PATH
DDIR=/

DIST=dict
echo "Extracting ${DIST}"
cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
