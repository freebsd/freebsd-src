# $FreeBSD$

PROG=	ibcacheedit
SRCS=	ibcacheedit.c
.if defined(TESTBUILD)
LDFLAGS= -libnetdisc
.else
LIBADD=	ibnetdisc
.endif
MAN=	ibcacheedit.8

.include <bsd.prog.mk>
