# $FreeBSD$

PROG=	dump_fts
SRCS=	dump_fts.c
.if defined(TESTBUILD)
LDFLAGS= -libnetdisc
.else
LIBADD= ibnetdisc
.endif
MAN=	dump_fts.8

.include <bsd.prog.mk>
