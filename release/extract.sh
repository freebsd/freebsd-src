#!/bin/sh
# $Id: extract.sh,v 1.15 1994/11/21 04:14:33 phk Exp $
PATH=/stand:$PATH
DDIR=/

if [ -f bindist.aa ] ; then
	# Temporary kludge for pathological bindist.
	if [ -f $DDIR/etc/myname ]; then
		cp $DDIR/etc/hosts $DDIR/etc/myname $DDIR/stand/etc
	fi
	if [ -f $DDIR/etc/defaultrouter ]; then
		cp $DDIR/etc/defaultrouter $DDIR/stand/etc
	fi
	echo; echo "Extracting bindist, please wait." 
	cat bindist.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	if [ -f $DDIR/stand/etc/myname ]; then
		# Add back what the bindist nuked.
		cp $DDIR/stand/etc/myname $DDIR/etc
		cat $DDIR/stand/etc/hosts >> $DDIR/etc/hosts
	fi
	if [ -f $DDIR/stand/etc/defaultrouter ]; then
		cp $DDIR/stand/etc/defaultrouter $DDIR/etc
	fi
	chmod 1777 /tmp
	rm -f /sys
	ln -s /usr/src/sys /sys
fi

for i in *.aa
do
	b=`basename $i .aa`
	if [ "$b" != bin_tgz ] ; then
		if [ "$b" = des ] ; then
			# We cannot replace /sbin/init while it runs
			# so move it out of the way for now
			mv /sbin/init /sbin/non_des_init
		fi
		echo "Extracting $b"
		cat $b.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
	fi
done
