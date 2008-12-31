# $FreeBSD: src/usr.sbin/ckdist/Makefile,v 1.5.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../usr.bin/cksum

PROG=	ckdist
SRCS=   ckdist.c crc.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
