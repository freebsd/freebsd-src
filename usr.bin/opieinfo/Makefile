# $FreeBSD: src/usr.bin/opieinfo/Makefile,v 1.3.2.1 1999/08/29 15:31:47 peter Exp $
#
OPIE_DIST?=	${.CURDIR}/../../contrib/opie

PROG=	opieinfo
SRCS=	opieinfo.c
MAN1=   opieinfo.1

CFLAGS+=-I${OPIE_DIST}

DPADD=	${LIBOPIE} ${LIBMD}
LDADD=	-lopie -lmd

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
