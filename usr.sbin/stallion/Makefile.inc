# $FreeBSD: src/usr.sbin/stallion/Makefile.inc,v 1.4 1999/08/28 01:20:05 peter Exp $

BOOTDIR=	/usr/libdata/stallion

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif
