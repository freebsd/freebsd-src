#!/bin/sh -
#
#	from named.reload	5.2 (Berkeley) 6/27/89
# $FreeBSD: src/usr.sbin/named.reload/named.reload.sh,v 1.2 1999/08/28 01:17:23 peter Exp $
#

exec %DESTSBIN%/%INDOT%ndc reload
