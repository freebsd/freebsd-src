#!/bin/sh
# $Id: extract.sh,v 1.17 1994/12/04 03:41:18 jkh Exp $
PATH=/stand:$PATH
DDIR=/

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
