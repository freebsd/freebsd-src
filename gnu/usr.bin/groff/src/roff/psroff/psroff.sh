#! /bin/sh -
#
# $FreeBSD: src/gnu/usr.bin/groff/src/roff/psroff/psroff.sh,v 1.2.36.1.6.1 2010/12/21 17:09:25 kensmith Exp $

exec groff -Tps -l -C ${1+"$@"}
