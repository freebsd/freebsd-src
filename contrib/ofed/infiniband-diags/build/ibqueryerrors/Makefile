# $FreeBSD$

PROG=	ibqueryerrors
SRCS=	ibqueryerrors.c
.if defined(TESTBUILD)
LDFLAGS= -libnetdisc
.else
LIBADD=	ibnetdisc
.endif
MAN=	ibqueryerrors.8

.include <bsd.prog.mk>
