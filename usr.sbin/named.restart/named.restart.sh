#!/bin/sh -
#
#	from named.restart	5.4 (Berkeley) 6/27/89
# $FreeBSD: src/usr.sbin/named.restart/named.restart.sh,v 1.2 1999/08/28 01:17:25 peter Exp $
#

exec %DESTSBIN%/%INDOT%ndc restart
