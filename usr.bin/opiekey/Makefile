#	$Id: Makefile,v 1.1.1.1.2.1 1997/02/22 05:00:19 nate Exp $
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
