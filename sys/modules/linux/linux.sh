#!/bin/sh

# $Id: linux.sh,v 1.6 1998/11/05 04:19:26 peter Exp $

FOUND=`kldstat -v | egrep 'linux(aout|elf)'`

if [ "x$FOUND" != x ] ; then           
	echo Linux driver already loaded
	exit 1
else    
	kldload linux                                                
fi
