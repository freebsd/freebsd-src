#!/bin/sh
#
if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
echo "Extracting xperimnt tarball into ${DESTDIR}/usr/local"
tar --unlink -xpzf xperimnt.tgz -C ${DESTDIR}/usr/local
exit 0
