#!/bin/sh

# $FreeBSD$

FOUND=`kldstat -v | egrep 'linux(aout|elf)'`

if [ "x$FOUND" != x ] ; then           
	echo Linux driver already loaded
	exit 1
else    
	kldload linux                                                
fi
