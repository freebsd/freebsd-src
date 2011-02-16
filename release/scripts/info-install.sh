#!/bin/sh
#
# $FreeBSD: src/release/scripts/info-install.sh,v 1.3.36.1.6.1 2010/12/21 17:09:25 kensmith Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat info.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
