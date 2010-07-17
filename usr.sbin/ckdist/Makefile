# $FreeBSD: src/usr.sbin/ckdist/Makefile,v 1.5.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../usr.bin/cksum

PROG=	ckdist
SRCS=   ckdist.c crc.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
