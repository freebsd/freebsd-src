#!/bin/sh
# $Id: extract_src.sh,v 1.5 1995/02/10 07:22:39 jkh Exp $
PATH=/stand:$PATH
DDIR=/usr/src

for DIST in base srcbin etc games gnu include lib libexec release sbin lkm \
	release share sys usrbin usrsbin; do
	if [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
ln -fs /usr/src/sys /sys
