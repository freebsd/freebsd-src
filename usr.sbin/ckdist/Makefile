# $FreeBSD: src/usr.sbin/ckdist/Makefile,v 1.5 2001/07/20 06:19:39 obrien Exp $

.PATH: ${.CURDIR}/../../usr.bin/cksum

PROG=	ckdist
SRCS=   ckdist.c crc.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
