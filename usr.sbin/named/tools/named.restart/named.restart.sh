#!/bin/sh -
#
#	from named.restart	5.4 (Berkeley) 6/27/89
#	$Id: named.restart.sh,v 1.2 1994/09/22 20:45:26 pst Exp $
#

# If there is a global system configuration file, suck it in.
if [ -f /etc/sysconfig ]; then
	. /etc/sysconfig
fi

PATH=%DESTSBIN%:/bin:/usr/bin

if [ -f %PIDDIR%/named.pid ]; then
	pid=`cat %PIDDIR%/named.pid`
	kill $pid
	sleep 5
fi

# $namedflags is imported from /etc/sysconfig
if [ "X${namedflags}" != "XNO" ]; then 
	exec %INDOT%named ${namedflags}
fi
