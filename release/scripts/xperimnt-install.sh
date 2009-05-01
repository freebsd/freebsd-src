#!/bin/sh
#
# $FreeBSD: src/release/scripts/xperimnt-install.sh,v 1.3.34.1 2009/04/15 03:14:26 kensmith Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
echo "Extracting xperimnt tarball into ${DESTDIR}/usr/local"
tar --unlink -xpzf xperimnt.tgz -C ${DESTDIR}/usr/local
exit 0
