# $FreeBSD: src/sbin/camcontrol/Makefile,v 1.11 2002/01/24 16:53:08 ru Exp $

MAINTAINER= ken@FreeBSD.org

PROG=	camcontrol
SRCS=	camcontrol.c util.c
.if !defined(RELEASE_CRUNCH)
SRCS+=	modeedit.c
.else
CFLAGS+= -DMINIMALISTIC
.endif
WARNS=	0
DPADD=	${LIBCAM} ${LIBSBUF}
LDADD=	-lcam -lsbuf
MAN=	camcontrol.8

.include <bsd.prog.mk>
