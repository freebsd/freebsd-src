# 	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.21 1999/04/13 18:22:57 peter Exp $

PROG=	config
CFLAGS+=-I. -I${.CURDIR}
SRCS=	config.y main.c lang.l mkioconf.c mkmakefile.c mkheaders.c \
	mkoptions.c mkswapconf.c y.tab.h
MAN8=	config.8
DPADD=	${LIBL}
LDADD=	-ll

mkmakefile.o: configvers.h

.include <bsd.prog.mk>
