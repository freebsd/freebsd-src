#!/bin/sh
# $Id: extract_src.sh,v 1.12 1995/03/28 08:01:47 phk Exp $
PATH=/stand:$PATH
DDIR=/usr/src

mkdir -p $DDIR

for T in s*.aa ; do
	DIST=`basename $T .aa`
	if [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? | 
			gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
rm -f /sys
ln -fs /usr/src/sys /sys
