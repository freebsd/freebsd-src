#!/bin/sh

# $Id$

FOUND=`kldstat -v | egrep 'svr4elf'`

if [ "x$FOUND" != x ] ; then           
	echo SysVR4 driver already loaded
	exit 1
else    
	kldload svr4
fi
