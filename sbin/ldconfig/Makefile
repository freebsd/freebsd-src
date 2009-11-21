# $FreeBSD: src/sbin/ldconfig/Makefile,v 1.20.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	ldconfig
SRCS=	elfhints.c ldconfig.c shlib.c support.c
LDDIR?= ${.CURDIR}/../../libexec/rtld-aout
WARNS?=	6
CFLAGS+=-I${LDDIR} -DFREEBSD_AOUT
MAN=	ldconfig.8

.PATH: ${LDDIR}

.include <bsd.prog.mk>
