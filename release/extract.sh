#!/bin/sh
if [ -f bin_tgz.aa ] ; then
	# Temporary kludge for pathological bindist.
	cp /etc/hosts /etc/myname /stand/etc
	echo; echo "Extracting bindist, please wait.  Ignore any messages from"
	echo "cpio saying \"No such file or directory\".  It doesn't know what"
	echo "it's talking about.."; echo
	cat bin_tgz.?? | zcat | ( cd / ; cpio -H tar -idumV )
	# Add back what the bindist nuked.
	cp /stand/etc/myname /etc
	cat /stand/etc/hosts >> /etc/hosts
fi
