# $FreeBSD$

PROG=	ibnetdiscover
SRCS=	ibnetdiscover.c
.if defined(TESTBUILD)
LDFLAGS= -libnetdisc
.else
LIBADD=	ibnetdisc
.endif
MAN=	ibnetdiscover.8

.include <bsd.prog.mk>
