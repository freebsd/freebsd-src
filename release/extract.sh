#!/bin/sh
# $Id: extract.sh,v 1.5 1994/11/10 02:24:47 phk Exp $

DDIR=/

if [ -f bin_tgz.aa ] ; then
	# Temporary kludge for pathological bindist.
	cp $DDIR/etc/hosts $DDIR/etc/myname $DDIR/stand/etc
	echo; echo "Extracting bindist, please wait.  Ignore any messages from"
	echo "cpio saying \"No such file or directory\".  It doesn't know what"
	echo "it's talking about.."; echo
	cat bin_tgz.?? | zcat | ( cd $DDIR ; cpio -H tar -idumV )
	# Add back what the bindist nuked.
	cp $DDIR/stand/etc/myname $DDIR/etc
	cat $DDIR/stand/etc/hosts >> $DDIR/etc/hosts
fi

for i in *.aa
do
	b=`basename $i .aa`
	if [ "$b" != bin_tgz ] ; then
		echo "Extracting $b"
		cat $b.?? | zcat | ( cd $DDIR ; tar xf - )
	fi
done
