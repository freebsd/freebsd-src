# 	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	config
MAN=	config.8
SRCS=	config.y main.c lang.l mkmakefile.c mkheaders.c \
	mkoptions.c y.tab.h

CFLAGS+= -I. -I${.CURDIR}

DPADD=	${LIBL}
LDADD=	-ll

mkmakefile.o: configvers.h

.include <bsd.prog.mk>
