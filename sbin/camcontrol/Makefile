# $FreeBSD: src/sbin/camcontrol/Makefile,v 1.3.2.3 2002/01/22 09:46:32 joerg Exp $
MAINTAINER=ken@FreeBSD.ORG

PROG=   camcontrol
SRCS=   camcontrol.c util.c
.if !defined(RELEASE_BUILD_FIXIT)
SRCS+=	modeedit.c
.endif
MAN=   camcontrol.8
SDIR=	${.CURDIR}/../../sys

CFLAGS+= -I${SDIR}
.if defined(RELEASE_BUILD_FIXIT)
CFLAGS+= -DMINIMALISTIC
.endif
DPADD=   ${LIBCAM}
LDADD+=  -lcam

.include <bsd.prog.mk>
