#!/bin/sh

# $FreeBSD: src/sys/modules/linux/linux.sh,v 1.6.2.1 1999/08/29 16:27:25 peter Exp $

FOUND=`kldstat -v | egrep 'linux(aout|elf)'`

if [ "x$FOUND" != x ] ; then           
	echo Linux driver already loaded
	exit 1
else    
	kldload linux                                                
fi
