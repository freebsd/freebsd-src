#!/bin/sh
# $Id: extract_bin.sh,v 1.3 1995/01/28 09:11:32 jkh Exp $
PATH=/stand:$PATH
DDIR=/

# Temporary kludge for pathological bindist.
if [ -f $DDIR/etc/sysconfig ]; then
	mv $DDIR/etc/sysconfig $DDIR/etc/sysconfig.save
fi
if [ -f $DDIR/etc/myname ]; then
	cp $DDIR/etc/hosts $DDIR/etc/myname $DDIR/stand/etc
fi
if [ -f $DDIR/etc/defaultrouter ]; then
	cp $DDIR/etc/defaultrouter $DDIR/stand/etc
fi
cat bin.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
if [ -f $DDIR/stand/etc/myname ]; then
	# Add back what the bindist nuked.
	cp $DDIR/stand/etc/myname $DDIR/etc
	cat $DDIR/stand/etc/hosts >> $DDIR/etc/hosts
fi
if [ -f $DDIR/stand/etc/defaultrouter ]; then
	cp $DDIR/stand/etc/defaultrouter $DDIR/etc
fi
if [ -f $DDIR/etc/sysconfig.save ]; then
	mv $DDIR/etc/sysconfig.save $DDIR/etc/sysconfig
fi

# Save some space in the tarballs by not sending these bloated files...
cd /usr/share/misc
for i in termcap vgrindefs
do
	/usr/bin/cap_mkdb $i
	/usr/sbin/chown bin.bin $i.db
	/bin/chmod 444 $i.db
done

chmod 1777 /tmp
