#!/bin/sh
#
# $FreeBSD: src/release/scripts/doc-install.sh,v 1.2.6.1 2002/08/08 08:23:53 ru Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
echo "You are about to extract the doc distribution into ${DESTDIR:-/} - are you SURE"
echo -n "you want to do this over your installed system (y/n)? "
read ans
if [ "$ans" = "y" ]; then
	cat doc.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
fi
