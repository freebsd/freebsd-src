#!/bin/sh
#
# $FreeBSD$
#

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
if [ $# -lt 1 ]; then
	echo "You must specify which kernel to extract."
	exit 1
fi

CONFIG=$1
BOOT=${DESTDIR}/boot
KERNEL=$BOOT/$CONFIG

if [ -d $KERNEL ]; then
	echo "You are about to extract the $CONFIG kernel distribution into $KERNEL - are you SURE"
	echo -n "you want to do this over your installed system (y/n)? "
	read ans 
else
	# new installation; do not prompt
	ans=y
fi
if [ "$ans" = "y" ]; then
	if [ -d $KERNEL ]; then
		sav=$KERNEL.sav
		if [ -d $sav ]; then
			# XXX remove stuff w/o a prompt
			echo "Removing existing $sav"
			rm -rf $sav
		fi
		echo "Saving existing $KERNEL as $sav"
		mv $KERNEL $sav
	fi
	# translate per Makefile:doTARBALL XXX are we sure to have tr?
	tn=`echo ${CONFIG} | tr 'A-Z' 'a-z'`
	cat $tn.?? | tar --unlink -xpzf - -C $BOOT
else
	echo "Installation of $CONFIG kernel distribution not done." 
fi
