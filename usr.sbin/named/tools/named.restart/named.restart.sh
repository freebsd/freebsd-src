#!/bin/sh -
#
#	from named.restart	5.4 (Berkeley) 6/27/89
#	named.restart.sh,v 1.2 1994/09/22 20:45:26 pst Exp
#

PATH=%DESTSBIN%:/bin:/usr/bin

pid=`cat %PIDDIR%/named.pid`
kill $pid
sleep 5
exec %INDOT%named 
