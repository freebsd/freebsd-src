#! /bin/sh -
#
# $FreeBSD: src/gnu/usr.bin/groff/src/roff/psroff/psroff.sh,v 1.2.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $

exec groff -Tps -l -C ${1+"$@"}
