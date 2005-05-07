#!/bin/sh
#
# $FreeBSD: src/release/scripts/compat4x-install.sh,v 1.1 2002/04/26 13:33:46 matusita Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat compat4x.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
