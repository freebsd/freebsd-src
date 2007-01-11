#!/bin/sh
#
# $FreeBSD: src/release/scripts/proflibs-install.sh,v 1.4.14.1 2006/08/28 08:16:06 ru Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat proflibs.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
