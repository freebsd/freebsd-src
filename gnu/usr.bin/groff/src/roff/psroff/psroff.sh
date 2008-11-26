#! /bin/sh -
#
# $FreeBSD: src/gnu/usr.bin/groff/src/roff/psroff/psroff.sh,v 1.2.30.1 2008/10/02 02:57:24 kensmith Exp $

exec groff -Tps -l -C ${1+"$@"}
