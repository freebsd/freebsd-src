#!/bin/sh
#
# $FreeBSD: src/release/scripts/proflibs-install.sh,v 1.4 2001/04/08 23:09:21 obrien Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat proflibs.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
cd ${DESTDIR:-/}usr/lib
if [ -f libdescrypt_p.a ]
then
	ln -f -s libdescrypt_p.a libcrypt_p.a
fi
exit 0
