#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	config
CFLAGS+=-I. -I${.CURDIR}
SRCS=	config.c main.c lang.c mkioconf.c mkmakefile.c mkglue.c mkheaders.c \
	mkswapconf.c
MAN8=	config.0
DPADD=	${LIBL}
LDADD=	-ll
CLEANFILES+=y.tab.h lang.c config.c y.tab.c

.include <bsd.prog.mk>
