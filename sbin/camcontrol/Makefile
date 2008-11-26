# $FreeBSD: src/sbin/camcontrol/Makefile,v 1.16.8.1.4.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	camcontrol
SRCS=	camcontrol.c util.c
.if !defined(RELEASE_CRUNCH)
SRCS+=	modeedit.c
.else
CFLAGS+= -DMINIMALISTIC
.endif
.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	6
.endif
DPADD=	${LIBCAM} ${LIBSBUF} ${LIBUTIL}
LDADD=	-lcam -lsbuf -lutil
MAN=	camcontrol.8

.include <bsd.prog.mk>
