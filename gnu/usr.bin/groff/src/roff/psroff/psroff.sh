#! /bin/sh -
#
# $FreeBSD: src/gnu/usr.bin/groff/src/roff/psroff/psroff.sh,v 1.2.40.1 2010/12/21 17:10:29 kensmith Exp $

exec groff -Tps -l -C ${1+"$@"}
