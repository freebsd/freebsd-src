#! /bin/sh -
#
# $FreeBSD: src/gnu/usr.bin/groff/src/roff/psroff/psroff.sh,v 1.1.10.1 2001/04/26 17:08:36 ru Exp $

exec groff -Tps -l -C ${1+"$@"}
