# 	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.20 1998/05/06 15:27:57 bde Exp $

PROG=	config
CFLAGS+=-I. -I${.CURDIR}
SRCS=	config.y main.c lang.l mkioconf.c mkmakefile.c mkglue.c mkheaders.c \
	mkoptions.c mkswapconf.c y.tab.h
MAN8=	config.8
DPADD=	${LIBL}
LDADD=	-ll

mkmakefile.o: configvers.h

.include <bsd.prog.mk>
