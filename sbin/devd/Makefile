# $FreeBSD: src/sbin/devd/Makefile,v 1.6.12.1 2008/10/02 02:57:24 kensmith Exp $

PROG_CXX=devd
SRCS=	devd.cc token.l parse.y y.tab.h
MAN=	devd.8 devd.conf.5
WARNS?=	1

NO_SHARED?=YES

DPADD=	${LIBL}
LDADD=	-ll

YFLAGS+=-v
CFLAGS+=-I. -I${.CURDIR}

CLEANFILES= y.output

.include <bsd.prog.mk>
