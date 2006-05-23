# $FreeBSD: src/sbin/ldconfig/Makefile,v 1.19 2004/02/23 20:13:53 johan Exp $

PROG=	ldconfig
SRCS=	elfhints.c ldconfig.c shlib.c support.c
LDDIR?= ${.CURDIR}/../../libexec/rtld-aout
CFLAGS+=-I${LDDIR} -DFREEBSD_AOUT
WARNS?=	0
MAN=	ldconfig.8

.PATH: ${LDDIR}

.include <bsd.prog.mk>
