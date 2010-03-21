#! /bin/sh -
#
# $FreeBSD: src/gnu/usr.bin/groff/src/roff/psroff/psroff.sh,v 1.2.38.1 2010/02/10 00:26:20 kensmith Exp $

exec groff -Tps -l -C ${1+"$@"}
