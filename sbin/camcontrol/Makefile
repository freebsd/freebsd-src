# $FreeBSD: src/sbin/camcontrol/Makefile,v 1.3.2.1 2000/08/14 05:42:32 kbyanc Exp $
MAINTAINER=ken@FreeBSD.ORG

PROG=   camcontrol
SRCS=   camcontrol.c modeedit.c util.c
MAN8=   camcontrol.8
SDIR=	${.CURDIR}/../../sys

CFLAGS+= -I${SDIR}
DPADD=   ${LIBCAM}
LDADD+=  -lcam

.include <bsd.prog.mk>
