#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/mtree/Makefile,v 1.25 2002/11/13 12:03:27 ru Exp $

.PATH: ${.CURDIR}/../../usr.bin/cksum

PROG=	mtree
MAN=	mtree.8
SRCS=	compare.c crc.c create.c excludes.c misc.c mtree.c spec.c verify.c

WARNS?=	4

CFLAGS+= -DMD5 -DSHA1 -DRMD160
DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
