#!/bin/sh
# $Id: extract_bin.sh,v 1.5 1995/04/09 03:44:03 jkh Exp $
set -e
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
	echo "running cap_mkdb $i"
	/usr/bin/cap_mkdb $i
	/usr/sbin/chown bin.bin $i.db
	/bin/chmod 444 $i.db
done

chmod 1777 /tmp
