#!/bin/sh
# $Id: extract_games.sh,v 1.2 1995/01/28 09:07:43 jkh Exp $
set -e
PATH=/stand:$PATH
DDIR=/

DIST=games
echo "Extracting ${DIST} - ignore any errors from cpio"
cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
