# $FreeBSD: src/sbin/ldconfig/Makefile,v 1.15 1999/08/28 00:13:20 peter Exp $

PROG=	ldconfig
SRCS=	elfhints.c ldconfig.c shlib.c support.c
LDDIR?= ${.CURDIR}/../../libexec/rtld-aout
CFLAGS+=-I${LDDIR} -DFREEBSD_AOUT
MAN8=	ldconfig.8

.PATH: ${LDDIR}

.include <bsd.prog.mk>
