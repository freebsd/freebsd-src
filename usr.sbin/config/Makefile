# 	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	config
CFLAGS+=-I. -I${.CURDIR}
SRCS=	config.c main.c lang.c mkioconf.c mkmakefile.c mkglue.c mkheaders.c \
	mkoptions.c mkswapconf.c y.tab.h
MAN8=	config.8
DPADD=	${LIBL}
LDADD=	-ll
CLEANFILES+= config.c lang.c lex.yy.c y.tab.[ch]

y.tab.h: config.c

.include <bsd.prog.mk>

$(OBJS): configvers.h
