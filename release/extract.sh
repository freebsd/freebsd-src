#!/bin/sh
# $Id: extract.sh,v 1.11 1994/11/17 11:53:12 jkh Exp $
PATH=/stand:$PATH
DDIR=/

if [ -f bin_tgz.aa ] ; then
	# Temporary kludge for pathological bindist.
	if [ -f $DDIR/etc/myname ]; then
		cp $DDIR/etc/hosts $DDIR/etc/myname $DDIR/stand/etc
	fi
	echo; echo "Extracting bindist, please wait." 
	cat bin_tgz.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	if [ -f $DDIR/stand/etc/myname ]; then
		# Add back what the bindist nuked.
		cp $DDIR/stand/etc/myname $DDIR/etc
		cat $DDIR/stand/etc/hosts >> $DDIR/etc/hosts
	fi
fi

for i in *.aa
do
	b=`basename $i .aa`
	if [ "$b" != bin_tgz ] ; then
		if [ "$b" = des_tgz ] ; then
			# We cannot replace /sbin/init while it runs
			# so move it out of the way for now
			mv /sbin/init /sbin/nondes_init
		fi
		echo "Extracting $b"
		cat $b.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
