# $FreeBSD: src/sbin/camcontrol/Makefile,v 1.16 2004/05/14 13:31:21 cognet Exp $

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
DPADD=	${LIBCAM} ${LIBSBUF}
LDADD=	-lcam -lsbuf
MAN=	camcontrol.8

.include <bsd.prog.mk>
