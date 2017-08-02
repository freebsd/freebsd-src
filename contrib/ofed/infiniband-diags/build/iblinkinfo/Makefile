# $FreeBSD$

PROG=	iblinkinfo
SRCS=	iblinkinfo.c
.if defined(TESTBUILD)
LDFLAGS= -libnetdisc
.else
LIBADD= ibnetdisc
.endif
MAN=	iblinkinfo.8

.include <bsd.prog.mk>
