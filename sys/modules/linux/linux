#!/bin/sh

# $Id: linux,v 1.5 1998/09/07 16:15:59 cracauer Exp $

FOUND=`kldstat -v | egrep 'linux(aout|elf)'`

if [ "x$FOUND" != x ] ; then           
	echo Linux driver already loaded
	exit 1
else    
	kldload linux                                                
fi
