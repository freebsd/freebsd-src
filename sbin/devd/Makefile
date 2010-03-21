# $FreeBSD: src/sbin/devd/Makefile,v 1.8.10.1 2010/02/10 00:26:20 kensmith Exp $

PROG_CXX=devd
SRCS=	devd.cc token.l parse.y y.tab.h
MAN=	devd.8 devd.conf.5
WARNS?=	4

NO_SHARED?=YES

DPADD=	${LIBL} ${LIBUTIL}
LDADD=	-ll -lutil

YFLAGS+=-v
CFLAGS+=-I. -I${.CURDIR}

CLEANFILES= y.output

.include <bsd.prog.mk>
