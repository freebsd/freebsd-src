#!/bin/sh
# $Id: extract_src.sh,v 1.4 1995/02/02 08:31:36 jkh Exp $
PATH=/stand:$PATH
DDIR=/usr/src

for DIST in base srcbin etc games gnu include lib libexec release sbin lkm \
	release share sys usrbin usrsbin; do
	if [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
rm -f /sys
ln -s /usr/src/sys /sys
