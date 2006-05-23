#!/bin/sh
#
# $FreeBSD: src/release/scripts/base-install.sh,v 1.6 2002/04/30 09:57:21 ru Exp $
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi

echo "You are about to extract the base distribution into ${DESTDIR:-/} - are you SURE"
echo -n "you want to do this over your installed system (y/n)? "
read ans 
if [ "$ans" = "y" ]; then
	cat base.?? | tar --unlink -xpzf - -C ${DESTDIR:-/}
fi
