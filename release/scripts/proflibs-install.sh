#!/bin/sh
#
# $FreeBSD: src/release/scripts/proflibs-install.sh,v 1.4.14.1.6.1 2008/10/02 02:57:24 kensmith Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat proflibs.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
