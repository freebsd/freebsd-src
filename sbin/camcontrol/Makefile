# $FreeBSD: src/sbin/camcontrol/Makefile,v 1.3.2.5 2002/04/19 13:46:38 ru Exp $

MAINTAINER= ken@FreeBSD.org

PROG=	camcontrol
SRCS=	camcontrol.c util.c
.if !defined(RELEASE_CRUNCH)
SRCS+=	modeedit.c
.else
CFLAGS+= -DMINIMALISTIC
.endif
DPADD=	${LIBCAM}
LDADD=	-lcam
MAN=	camcontrol.8

.include <bsd.prog.mk>
