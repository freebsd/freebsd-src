# $FreeBSD: src/usr.bin/opiekey/Makefile,v 1.3.2.1 1999/08/29 15:31:48 peter Exp $
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
