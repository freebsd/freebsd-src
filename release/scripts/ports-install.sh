#!/bin/sh
#
# $FreeBSD: src/release/scripts/ports-install.sh,v 1.3.36.2.4.1 2012/03/03 06:15:13 kensmith Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
echo "Extracting ports tarball into ${DESTDIR}/usr"
tar --unlink -xpzf ports.tgz -C ${DESTDIR}/usr
exit 0
