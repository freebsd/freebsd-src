# $FreeBSD: src/usr.bin/opieinfo/Makefile,v 1.4 1999/08/28 01:04:45 peter Exp $
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
