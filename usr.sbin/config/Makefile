# 	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	config
MAN=	config.5 config.8
SRCS=	config.y main.c lang.l mkmakefile.c mkheaders.c \
	mkoptions.c y.tab.h kernconf.c

kernconf.c: kernconf.tmpl
	file2c 'char kernconfstr[] = {' ',0};' < ${.CURDIR}/kernconf.tmpl > kernconf.c

CFLAGS+= -I. -I${.CURDIR}

NO_WMISSING_VARIABLE_DECLARATIONS=

LIBADD=	l sbuf

CLEANFILES+=	kernconf.c

mkmakefile.o: configvers.h

.include <bsd.prog.mk>
