# 	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	config
CFLAGS+=-I. -I${.CURDIR}
SRCS=	config.c main.c lang.c mkioconf.c mkmakefile.c mkglue.c mkheaders.c \
	mkswapconf.c
MAN8=	config.8
DPADD=	${LIBL}
LDADD=	-ll
CLEANFILES+= config.c lang.c lex.yy.c y.tab.[ch]
.if defined(CONFIG_NO_CLOBBER_EVER)
CFLAGS+=-DNO_CLOBBER_EVER
.endif

.include <bsd.prog.mk>
