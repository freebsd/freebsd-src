# $FreeBSD: src/usr.sbin/ckdist/Makefile,v 1.5.40.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../usr.bin/cksum

PROG=	ckdist
SRCS=   ckdist.c crc.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
