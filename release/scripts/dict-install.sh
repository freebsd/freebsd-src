#!/bin/sh
#
# $FreeBSD: src/release/scripts/dict-install.sh,v 1.3.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat dict.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
