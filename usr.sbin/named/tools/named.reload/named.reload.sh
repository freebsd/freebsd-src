#!/bin/sh -
#
#	from named.reload	5.2 (Berkeley) 6/27/89
#	$Id: named.reload.sh,v 4.9.1.2 1993/09/08 00:01:17 vixie Exp $
#
kill -HUP `cat %PIDDIR%/named.pid`
