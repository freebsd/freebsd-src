# 	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/config/Makefile,v 1.34.20.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	config
MAN=	config.8
SRCS=	config.y main.c lang.l mkmakefile.c mkheaders.c \
	mkoptions.c y.tab.h

WARNS?=	6
CFLAGS+= -I. -I${.CURDIR}

DPADD=	${LIBL}
LDADD=	-ll

mkmakefile.o: configvers.h

.include <bsd.prog.mk>
