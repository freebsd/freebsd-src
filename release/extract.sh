#!/bin/sh
if [ -f bin_tgz.aa ] ; then
	echo; echo "Extracting bindist, please wait.  Ignore any messages from"
	echo "cpio saying \"No such file or directory\".  It doesn't know what"
	echo "it's talking about.."; echo
	cat bin_tgz.?? | zcat | ( cd / ; cpio -H tar -idumV )
fi
