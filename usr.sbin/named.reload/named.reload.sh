#!/bin/sh -
#
#	from named.reload	5.2 (Berkeley) 6/27/89
# $FreeBSD: src/usr.sbin/named.reload/named.reload.sh,v 1.1.2.1 1999/08/29 15:44:24 peter Exp $
#

exec %DESTSBIN%/%INDOT%ndc reload
