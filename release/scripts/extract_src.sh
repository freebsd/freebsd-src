#!/bin/sh
# $Id: extract_src.sh,v 1.1 1995/01/14 07:41:46 jkh Exp $
PATH=/stand:$PATH
DDIR=/

for DIST in base bin etc games gnu include lib libexec release sbin lkm \
	release share sys usrbin usrsbin; do
	if [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
