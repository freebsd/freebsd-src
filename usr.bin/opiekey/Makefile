# $FreeBSD: src/usr.bin/opiekey/Makefile,v 1.4 1999/08/28 01:04:46 peter Exp $
#
OPIE_DIST?=	${.CURDIR}/../../contrib/opie

PROG=	opiekey
SRCS=	opiekey.c
MAN1=   opiekey.1

CFLAGS+=-I${OPIE_DIST}

DPADD=	${LIBOPIE} ${LIBMD}
LDADD=	-lopie -lmd

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
