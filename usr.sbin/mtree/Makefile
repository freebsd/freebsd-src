#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.PATH: ${.CURDIR}/../../usr.bin/cksum

PROG=	mtree
MAN=	mtree.8 mtree.5
SRCS=	compare.c crc.c create.c excludes.c misc.c mtree.c spec.c verify.c
SRCS+=	specspec.c

CFLAGS+= -DMD5 -DSHA1 -DRMD160 -DSHA256
DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
