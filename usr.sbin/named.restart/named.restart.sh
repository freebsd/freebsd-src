#!/bin/sh -
#
#	from named.restart	5.4 (Berkeley) 6/27/89
# $FreeBSD: src/usr.sbin/named.restart/named.restart.sh,v 1.1.2.1 1999/08/29 15:44:26 peter Exp $
#

exec %DESTSBIN%/%INDOT%ndc restart
