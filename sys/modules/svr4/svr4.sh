#!/bin/sh

# $FreeBSD$

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
