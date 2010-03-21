# $FreeBSD: src/usr.sbin/ckdist/Makefile,v 1.5.38.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../usr.bin/cksum

PROG=	ckdist
SRCS=   ckdist.c crc.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
