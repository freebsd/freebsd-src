#	$Id: Makefile,v 1.3 1997/02/22 19:56:31 peter Exp $
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
