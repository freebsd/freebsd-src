#! /bin/sh -
#
# $FreeBSD: src/gnu/usr.bin/groff/src/roff/psroff/psroff.sh,v 1.2.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

exec groff -Tps -l -C ${1+"$@"}
