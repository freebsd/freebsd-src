# 	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/config/Makefile,v 1.20.2.1 1999/08/29 15:40:24 peter Exp $

PROG=	config
CFLAGS+=-I. -I${.CURDIR}
SRCS=	config.y main.c lang.l mkioconf.c mkmakefile.c mkglue.c mkheaders.c \
	mkoptions.c mkswapconf.c y.tab.h
MAN8=	config.8
DPADD=	${LIBL}
LDADD=	-ll

.include <bsd.prog.mk>

$(OBJS): configvers.h
