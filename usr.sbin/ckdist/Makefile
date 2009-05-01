# $FreeBSD: src/usr.sbin/ckdist/Makefile,v 1.5.34.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../usr.bin/cksum

PROG=	ckdist
SRCS=   ckdist.c crc.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
