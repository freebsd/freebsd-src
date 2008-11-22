# $FreeBSD$

PROG=	kbdio
NO_MAN=
SRCS=	kbdio.y lex.l y.tab.h

CFLAGS+= -I${.OBJDIR} -I${.CURDIR} #-g

#YACC=	bison
#YFLAGS+= -yd	# Bison only

YFLAGS+= -v	# verbose
LFLAGS+= -I

DPADD=	${LIBM} ${LIBY} ${LIBL}
LDADD=	-lm -ly -ll

CLEANFILES+= y.output	# comment file from bison

install:
	@${ECHO} "kbdio is not installed automatically ...."

.include <bsd.prog.mk>
