#!/bin/sh
#
# $FreeBSD: src/release/scripts/compat1x-install.sh,v 1.3.6.1 2002/08/08 08:23:53 ru Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat compat1x.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
