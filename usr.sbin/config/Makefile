#	@(#)Makefile	5.12 (Berkeley) 1/16/91

PROG=	config
CFLAGS+=-I. -I${.CURDIR}
SRCS=	config.c main.c lang.c mkioconf.c mkmakefile.c mkglue.c mkheaders.c \
	mkswapconf.c
MAN8=	config.8
DPADD=	${LIBLN}
LDADD=	-lln
CLEANFILES+=y.tab.h lang.c config.c y.tab.c

.include <bsd.prog.mk>
