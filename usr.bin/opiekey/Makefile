#	$FreeBSD$
#
OPIE_DIST?=../../contrib/opie

PROG=	opiekey
SRCS=	opiekey.c
MAN1=   opiekey.1

CFLAGS+=-I${OPIE_DIST}

DPADD=	${LIBOPIE} ${LIBMD}
LDADD=	-lopie -lmd

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
