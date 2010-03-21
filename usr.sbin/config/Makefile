# 	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/config/Makefile,v 1.38.10.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	config
MAN=	config.5 config.8
SRCS=	config.y main.c lang.l mkmakefile.c mkheaders.c \
	mkoptions.c y.tab.h kernconf.c

kernconf.c: kernconf.tmpl
	file2c 'char kernconfstr[] = {' ',0};' < ${.CURDIR}/kernconf.tmpl > kernconf.c

WARNS?=	6
CFLAGS+= -I. -I${.CURDIR}

DPADD=	${LIBL} ${LIBSBUF}
LDADD=	-ll -lsbuf

CLEANFILES+=	kernconf.c

mkmakefile.o: configvers.h

.include <bsd.prog.mk>
