#	$FreeBSD$
#
OPIE_DIST?=../../contrib/opie

PROG=	opiepasswd
SRCS=	opiepasswd.c
MAN1=   opiepasswd.1

CFLAGS+=-I${OPIE_DIST}

DPADD=	${LIBOPIE} ${LIBMD}
LDADD=	-lopie -lmd

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
