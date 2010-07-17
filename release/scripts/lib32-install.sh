#!/bin/sh
#
# $FreeBSD: src/release/scripts/lib32-install.sh,v 1.1.12.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat lib32.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
