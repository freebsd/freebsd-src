#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.7 1999/02/26 18:44:56 wollman Exp $

PROG=	mtree
SRCS=	compare.c crc.c create.c misc.c mtree.c spec.c verify.c
MAN8=	mtree.8
.PATH:	${.CURDIR}/../../usr.bin/cksum

DPADD+=	${LIBMD}
LDADD+=	-lmd
CFLAGS+= -DMD5

.if !defined(WORLD)
CFLAGS+= -DSHA1 -DRMD160
.endif

.include <bsd.prog.mk>
