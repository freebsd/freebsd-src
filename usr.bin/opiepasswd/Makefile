# $FreeBSD: src/usr.bin/opiepasswd/Makefile,v 1.3.2.1 1999/08/29 15:31:50 peter Exp $
#
OPIE_DIST?=	${.CURDIR}/../../contrib/opie

PROG=	opiepasswd
SRCS=	opiepasswd.c
MAN1=   opiepasswd.1

CFLAGS+=-I${OPIE_DIST}

DPADD=	${LIBOPIE} ${LIBMD}
LDADD=	-lopie -lmd

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
