#!/bin/sh
#
# $FreeBSD: src/release/scripts/ports-install.sh,v 1.3.30.1 2008/10/02 02:57:24 kensmith Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
echo "Extracting ports tarball into ${DESTDIR}/usr"
cat ports.tgz | tar --unlink -xpzf - -C ${DESTDIR}/usr
exit 0
