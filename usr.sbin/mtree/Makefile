#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/mtree/Makefile,v 1.6.2.1 1999/08/29 15:44:16 peter Exp $

PROG=	mtree
SRCS=	compare.c crc.c create.c misc.c mtree.c spec.c verify.c
MAN8=	mtree.8
.PATH:	${.CURDIR}/../../usr.bin/cksum

DPADD+=	${LIBMD}
LDADD+=	-lmd

.include <bsd.prog.mk>
