#!/bin/sh
#
# $FreeBSD: src/release/scripts/commerce-install.sh,v 1.3.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
echo "Extracting commerce tarball into ${DESTDIR}/usr/local"
tar --unlink -xpzf commerce.tgz -C ${DESTDIR}/usr/local
exit 0
