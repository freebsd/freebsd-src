# $FreeBSD: src/usr.sbin/ckdist/Makefile,v 1.5.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../usr.bin/cksum

PROG=	ckdist
SRCS=   ckdist.c crc.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
