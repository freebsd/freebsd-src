# $FreeBSD: src/usr.sbin/ckdist/Makefile,v 1.1.1.1.2.1 1999/08/29 15:40:22 peter Exp $

PROG=	ckdist

.PATH: ${.CURDIR}/../../usr.bin/cksum

SRCS=   ckdist.c crc.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
