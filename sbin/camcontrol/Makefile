# $FreeBSD: src/sbin/camcontrol/Makefile,v 1.3.2.2 2001/04/25 10:58:12 ru Exp $
MAINTAINER=ken@FreeBSD.ORG

PROG=   camcontrol
SRCS=   camcontrol.c modeedit.c util.c
MAN=   camcontrol.8
SDIR=	${.CURDIR}/../../sys

CFLAGS+= -I${SDIR}
DPADD=   ${LIBCAM}
LDADD+=  -lcam

.include <bsd.prog.mk>
