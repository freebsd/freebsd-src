# $FreeBSD$

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
