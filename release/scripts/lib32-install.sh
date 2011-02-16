#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-install.sh,v 1.1.16.1 2010/12/21 17:10:29 kensmith Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat lib32.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
