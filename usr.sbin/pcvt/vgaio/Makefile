# $FreeBSD$
PROG=	vgaio
MAN=	vgaio.8

SRCS=	vgaio.y lex.l y.tab.h
YACC=	yacc

#YFLAGS+= -yd	# Bison
#YFLAGS+= -v	# verbose

LFLAGS+= -I

DPADD=	${LIBM} ${LIBY} ${LIBL}
LDADD=	-lm -ly -ll

CFLAGS+= -I${.OBJDIR} -I${.CURDIR}

CLEANFILES+= y.output	# comment file from bison

install:
	@${ECHO} "vgaio is not installed automatically ...."

.include <bsd.prog.mk>
