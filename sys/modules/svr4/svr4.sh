#!/bin/sh

# $FreeBSD: src/sys/modules/svr4/svr4.sh,v 1.3 1999/08/28 00:47:42 peter Exp $

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
