# 	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/config/Makefile,v 1.26.2.1 2001/04/25 12:09:23 ru Exp $

PROG=	config
CFLAGS+=-I. -I${.CURDIR}
CFLAGS+=-Wall -Wunused -Wmissing-prototypes -Wredundant-decls
SRCS=	config.y main.c lang.l mkioconf.c mkmakefile.c mkheaders.c \
	mkoptions.c y.tab.h
MAN=	config.8
DPADD=	${LIBL}
LDADD=	-ll

mkmakefile.o: configvers.h

.include <bsd.prog.mk>
