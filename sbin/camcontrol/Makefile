# $FreeBSD$
MAINTAINER=ken@FreeBSD.ORG

PROG=   camcontrol
SRCS=   camcontrol.c modeedit.c
MAN8=   camcontrol.8
SDIR=	${.CURDIR}/../../sys

CFLAGS+= -I${SDIR}
DPADD=   ${LIBCAM}
LDADD+=  -lcam

.include <bsd.prog.mk>
