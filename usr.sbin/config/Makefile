# 	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/config/Makefile,v 1.31 2001/07/20 06:19:40 obrien Exp $

PROG=	config
MAN=	config.8
SRCS=	config.y main.c lang.l mkmakefile.c mkheaders.c \
	mkoptions.c y.tab.h

CFLAGS+= -I. -I${.CURDIR}
CFLAGS+= ${BDECFLAGS}

DPADD=	${LIBL}
LDADD=	-ll

mkmakefile.o: configvers.h

.include <bsd.prog.mk>
