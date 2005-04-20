# $FreeBSD$

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
