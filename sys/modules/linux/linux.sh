#!/bin/sh

# $FreeBSD$

FOUND=`kldstat -v | egrep 'linux(aout|elf)'`

exitcode=0

if [ "x$FOUND" != x ] ; then           
	echo Linux driver already loaded
	exitcode=1
else    
	kldload linux                                                
	exitcode=$?
fi

if [ -f /compat/linux/sbin/ldconfig ] ; then
	/compat/linux/sbin/ldconfig
fi

exit $exitcode
