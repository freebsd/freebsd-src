#!/bin/sh
# $Id: extract_src.sh,v 1.7 1995/03/24 00:16:51 phk Exp $
PATH=/stand:$PATH
DDIR=/usr/src

for DIST in base srcbin etc games gnu include lib libexec release sbin lkm \
	release share sys usrbin usrsbin; do
	if [ -f ${DIST}/${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}/${DIST}.?? 
			| gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	elif [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? 
			| gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
ln -fs /usr/src/sys /sys
