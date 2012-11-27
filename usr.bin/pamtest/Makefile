# $FreeBSD$

OPENPAM=	${.CURDIR}/../../contrib/openpam
.PATH: ${OPENPAM}/include ${OPENPAM}/bin/pamtest

PROG= pamtest
SRCS= pamtest.c
DPADD= ${LIBPAM}
LDADD= ${MINUSLPAM}

.include <bsd.prog.mk>
