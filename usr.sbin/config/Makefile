# 	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.23 1999/04/18 13:36:28 peter Exp $

PROG=	config
CFLAGS+=-I. -I${.CURDIR}
#CFLAGS+=-Wall -Wunused -Wmissing-prototypes -Wredundant-decls
SRCS=	config.y main.c lang.l mkioconf.c mkmakefile.c mkheaders.c \
	mkoptions.c y.tab.h
MAN8=	config.8
DPADD=	${LIBL}
LDADD=	-ll

mkmakefile.o: configvers.h

.include <bsd.prog.mk>
