# $FreeBSD: src/sbin/devd/Makefile,v 1.3 2002/12/07 08:04:36 imp Exp $

PROG_CXX=devd
SRCS=	devd.cc token.l parse.y y.tab.h
MAN=	devd.8 devd.conf.5
#WARNS?=	5

DPADD=	${LIBL}
LDADD=	-ll

YFLAGS+=-v
CFLAGS+=-I. -I${.CURDIR}

CLEANFILES= y.output

.include <bsd.prog.mk>
