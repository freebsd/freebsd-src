#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.PATH: ${.CURDIR}/../../usr.bin/cksum

PROG=	mtree
MAN=	mtree.8
SRCS=	compare.c crc.c create.c excludes.c misc.c mtree.c spec.c verify.c

.if defined(BOOTSTRAPPING)
.PATH: ${.CURDIR}/../../lib/libc/gen
SRCS+=	strtofflags.c
.else
CFLAGS+= -DMD5 -DSHA1 -DRMD160
DPADD=	${LIBMD}
LDADD=	-lmd
.endif

.include <bsd.prog.mk>
