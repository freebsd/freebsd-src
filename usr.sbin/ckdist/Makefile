# $FreeBSD: src/usr.sbin/ckdist/Makefile,v 1.2 1999/08/28 01:15:46 peter Exp $

PROG=	ckdist

.PATH: ${.CURDIR}/../../usr.bin/cksum

SRCS=   ckdist.c crc.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
