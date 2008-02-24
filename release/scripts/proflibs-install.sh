#!/bin/sh
#
# $FreeBSD: src/release/scripts/proflibs-install.sh,v 1.6 2006/08/28 08:12:49 ru Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat proflibs.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
