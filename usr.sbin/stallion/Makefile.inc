# $FreeBSD: src/usr.sbin/stallion/Makefile.inc,v 1.3.2.1 1999/08/29 15:48:21 peter Exp $

BOOTDIR=	/usr/libdata/stallion

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif
