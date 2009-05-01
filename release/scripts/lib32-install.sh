#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-install.sh,v 1.1.10.1 2009/04/15 03:14:26 kensmith Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat lib32.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
