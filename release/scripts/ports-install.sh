#!/bin/sh
#
if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
cat ports.tgz | tar --unlink -xpzf - -C /usr
exit 0
