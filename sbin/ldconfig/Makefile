# $FreeBSD: src/sbin/ldconfig/Makefile,v 1.20.24.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	ldconfig
SRCS=	elfhints.c ldconfig.c shlib.c support.c
LDDIR?= ${.CURDIR}/../../libexec/rtld-aout
WARNS?=	6
CFLAGS+=-I${LDDIR} -DFREEBSD_AOUT
MAN=	ldconfig.8

.PATH: ${LDDIR}

.include <bsd.prog.mk>
