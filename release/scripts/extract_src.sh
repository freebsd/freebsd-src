#!/bin/sh
# $Id: extract_src.sh,v 1.9 1995/03/24 02:45:34 phk Exp $
PATH=/stand:$PATH
DDIR=/usr/src

for T in src*.aa ; do
	DIST=`basename $T .aa`
	if [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? | 
			gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
ln -fs /usr/src/sys /sys
