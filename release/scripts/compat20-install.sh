#!/bin/sh
#
if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
tar --unlink -xpzf compat20.tgz -C /
exit 0
