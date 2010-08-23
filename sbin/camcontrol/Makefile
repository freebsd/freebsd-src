# $FreeBSD$

PROG=	camcontrol
SRCS=	camcontrol.c util.c
.if !defined(RELEASE_CRUNCH)
SRCS+=	modeedit.c
.else
CFLAGS+= -DMINIMALISTIC
.endif
# This is verboten
.if ${MACHINE_CPUARCH} == "arm"
WARNS?= 3
.endif
DPADD=	${LIBCAM} ${LIBSBUF} ${LIBUTIL}
LDADD=	-lcam -lsbuf -lutil
MAN=	camcontrol.8

.include <bsd.prog.mk>
