#!/bin/sh -
#
#	from named.reload	5.2 (Berkeley) 6/27/89
#	named.reload.sh,v 1.2 1994/09/22 20:45:23 pst Exp
#
kill -HUP `cat %PIDDIR%/named.pid`
