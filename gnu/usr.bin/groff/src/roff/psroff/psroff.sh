#! /bin/sh -
#
# $FreeBSD: src/gnu/usr.bin/groff/src/roff/psroff/psroff.sh,v 1.2.34.1 2009/04/15 03:14:26 kensmith Exp $

exec groff -Tps -l -C ${1+"$@"}
