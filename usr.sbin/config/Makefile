# 	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id$

PROG=	config
CFLAGS+=-I. -I${.CURDIR}
SRCS=	config.y main.c lang.l mkioconf.c mkmakefile.c mkglue.c mkheaders.c \
	mkoptions.c mkswapconf.c
MAN8=	config.8
DPADD=	${LIBL}
LDADD=	-ll

.include <bsd.prog.mk>

$(OBJS): configvers.h
