#!/bin/sh
# $Id: extract_src.sh,v 1.10 1995/03/24 03:38:48 phk Exp $
PATH=/stand:$PATH
DDIR=/usr/src

mkdir -p $DDIR

for T in src*.aa ; do
	DIST=`basename $T .aa`
	if [ -f ${DIST}.aa ]; then
		echo "Extracting ${DIST} sources"
		cat ${DIST}.?? | 
			gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
ln -fs /usr/src/sys /sys
