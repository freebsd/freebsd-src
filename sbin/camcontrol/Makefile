# $FreeBSD: src/sbin/camcontrol/Makefile,v 1.13 2003/05/02 06:56:32 obrien Exp $

MAINTAINER= ken@FreeBSD.org

PROG=	camcontrol
SRCS=	camcontrol.c util.c
.if !defined(RELEASE_CRUNCH)
SRCS+=	modeedit.c
.else
CFLAGS+= -DMINIMALISTIC
.endif
WARNS=	1
DPADD=	${LIBCAM} ${LIBSBUF}
LDADD=	-lcam -lsbuf
MAN=	camcontrol.8

.include <bsd.prog.mk>
