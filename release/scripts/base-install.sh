#!/bin/sh
#
if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
echo "You are about to extract the base distribution into / - are you SURE"
echo "you want to do this over your installed system?  If not, hit ^C now!"
read junk
cat bin.?? | tar --unlink -xpzf - -C /
