# $FreeBSD: src/sbin/ldconfig/Makefile,v 1.20.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	ldconfig
SRCS=	elfhints.c ldconfig.c shlib.c support.c
LDDIR?= ${.CURDIR}/../../libexec/rtld-aout
WARNS?=	6
CFLAGS+=-I${LDDIR} -DFREEBSD_AOUT
MAN=	ldconfig.8

.PATH: ${LDDIR}

.include <bsd.prog.mk>
