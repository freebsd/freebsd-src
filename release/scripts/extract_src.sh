#!/bin/sh
# $Id: extract_src.sh,v 1.3 1995/01/29 08:38:22 jkh Exp $
PATH=/stand:$PATH
DDIR=/usr/src

for DIST in base srcbin etc games gnu include lib libexec release sbin lkm \
	release share sys usrbin usrsbin; do
	if [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
