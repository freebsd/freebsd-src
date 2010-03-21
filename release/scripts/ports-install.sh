#!/bin/sh
#
# $FreeBSD: src/release/scripts/ports-install.sh,v 1.3.38.1 2010/02/10 00:26:20 kensmith Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
echo "Extracting ports tarball into ${DESTDIR}/usr"
cat ports.tgz | tar --unlink -xpzf - -C ${DESTDIR}/usr
exit 0
