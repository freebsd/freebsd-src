# $FreeBSD$
MAINTAINER=ken@FreeBSD.ORG

PROG=   camcontrol
SRCS=   camcontrol.c modeedit.c util.c
MAN=   camcontrol.8
SDIR=	${.CURDIR}/../../sys

CFLAGS+= -I${SDIR}
DPADD=   ${LIBCAM} ${LIBSBUF}
LDADD+=  -lcam -lsbuf

.include <bsd.prog.mk>
