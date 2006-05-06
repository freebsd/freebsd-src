#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/mtree/Makefile,v 1.27 2005/03/29 11:44:17 tobez Exp $

.PATH: ${.CURDIR}/../../usr.bin/cksum

PROG=	mtree
MAN=	mtree.8
SRCS=	compare.c crc.c create.c excludes.c misc.c mtree.c spec.c verify.c
SRCS+=	specspec.c

WARNS?=	4

CFLAGS+= -DMD5 -DSHA1 -DRMD160 -DSHA256
DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
