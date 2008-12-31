#! /bin/sh -
#
# $FreeBSD: src/gnu/usr.bin/groff/src/roff/psroff/psroff.sh,v 1.2.32.1 2008/11/25 02:59:29 kensmith Exp $

exec groff -Tps -l -C ${1+"$@"}
