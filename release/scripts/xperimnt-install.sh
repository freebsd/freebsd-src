#!/bin/sh
#
# $FreeBSD: src/release/scripts/xperimnt-install.sh,v 1.3.36.1.2.1 2009/10/25 01:10:29 kensmith Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
echo "Extracting xperimnt tarball into ${DESTDIR}/usr/local"
tar --unlink -xpzf xperimnt.tgz -C ${DESTDIR}/usr/local
exit 0
