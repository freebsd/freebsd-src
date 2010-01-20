#!/bin/sh
#
# $FreeBSD$
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat info.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
exit 0
