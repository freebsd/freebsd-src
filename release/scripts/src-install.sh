#!/bin/sh
#
if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
if [ $# -lt 1 ]; then
	echo "You must specify which components of src to extract"
	echo "possible subcomponents are:"
	echo
	echo "base bin etc games gnu include lib libexec lkm release"
	echo "sbin share smailcf sys subin susbin"
	echo
	exit 1
fi

for i in $*; do
	echo "Extracting source component: $i"
	cat s${i}.?? | tar --unlink -xpzf - -C /usr/src
done
exit 0
