# $FreeBSD: src/sbin/devd/Makefile,v 1.4 2003/04/01 11:11:38 ru Exp $

PROG_CXX=devd
SRCS=	devd.cc token.l parse.y y.tab.h
MAN=	devd.8 devd.conf.5
WARNS?=	1

DPADD=	${LIBL}
LDADD=	-ll

YFLAGS+=-v
CFLAGS+=-I. -I${.CURDIR}

CLEANFILES= y.output

.include <bsd.prog.mk>
