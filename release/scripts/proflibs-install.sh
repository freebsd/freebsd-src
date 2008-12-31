#!/bin/sh
#
# $FreeBSD: src/release/scripts/proflibs-install.sh,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat proflibs.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
