#!/bin/sh
# $Id: extract_src.sh,v 1.2 1995/01/27 07:49:46 jkh Exp $
PATH=/stand:$PATH
DDIR=/

for DIST in base srcbin etc games gnu include lib libexec release sbin lkm \
	release share sys usrbin usrsbin; do
	if [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
