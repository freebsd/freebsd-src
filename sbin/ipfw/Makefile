# $FreeBSD$

PROG=	ipfw
MAN=	ipfw.8
CFLAGS+=-Wall
.if defined(IPFW2)
SRCS=	ipfw2.c
CFLAGS+= -DIPFW2
.endif

.include <bsd.prog.mk>
