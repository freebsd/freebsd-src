#!/bin/sh
# $Id: extract_bin.sh,v 1.4 1995/03/28 18:14:10 phk Exp $
PATH=/stand:$PATH
DDIR=/

# Temporary kludge for pathological bindist.
if [ -f $DDIR/etc/sysconfig ]; then
	mv $DDIR/etc/sysconfig $DDIR/etc/sysconfig.save
fi
cat bin.?? | gzip -c -d | ( cd $DDIR; cpio -H tar -imdu )
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
