#!/bin/sh -
#
#	from named.restart	5.4 (Berkeley) 6/27/89
#	$Id: named.restart.sh,v 4.9.1.5 1994/04/09 03:43:17 vixie Exp $
#

PATH=%DESTSBIN%:/bin:/usr/bin

pid=`cat %PIDDIR%/named.pid`
kill $pid
sleep 5
exec %INDOT%named 
