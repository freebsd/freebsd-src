# $FreeBSD$
#
OPIE_DIST?=	${.CURDIR}/../../contrib/opie

PROG=	opiepasswd

CFLAGS+=-I${OPIE_DIST}

.if defined(WANT_INSECURE_OPIE) && ${WANT_INSECURE_OPIE} == true
CFLAGS+=-DINSECURE_OVERRIDE
.endif

DPADD=	${LIBOPIE} ${LIBMD}
LDADD=	-lopie -lmd

BINMODE=4555
INSTALLFLAGS=-fschg

.PATH:	${OPIE_DIST}

.include <bsd.prog.mk>
