# $FreeBSD$
#
OPIE_DIST?=	${.CURDIR}/../../contrib/opie

PROG=	opiepasswd

CFLAGS+=-I${OPIE_DIST}

DPADD=	${LIBOPIE} ${LIBMD}
LDADD=	-lopie -lmd

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
