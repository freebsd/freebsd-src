#!/bin/sh

# $Id: svr4.sh,v 1.1 1999/01/30 06:29:48 newton Exp $

STREAMS=`kldstat -v | egrep 'streams'`
SVR4=`kldstat -v | egrep 'svr4elf'`

if [ "x$SVR4" != x ] ; then           
	echo SysVR4 driver already loaded
	exit 1
else    
	if [ "x$STREAMS" = x ] ; then
		kldload streams
		echo "Loaded SysVR4 STREAMS driver"
	fi
	kldload svr4
	echo "Loaded SysVR4 emulator"
fi
