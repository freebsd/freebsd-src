#	$Id: Makefile,v 1.1.1.1.2.1 1997/02/22 04:59:30 nate Exp $
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
