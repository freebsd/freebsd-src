#!/bin/sh
#
if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
echo "You are about to extract the DES distribution into / - are you SURE"
echo "you want to do this over your installed system?  If not, hit ^C now!"
read junk
cat des.?? | tar --unlink -xpzf - -C /
cat krb.?? | tar --unlink -xpzf - -C /
echo -n "Do you want to install the DES sources (y/n)? "
read ans
if [ "$ans" = "y" ]; then
	cat sebones.?? | tar --unlink -xpzf - -C /usr/src
	cat ssecure.?? | tar --unlink -xpzf - -C /usr/src
fi
exit 0
