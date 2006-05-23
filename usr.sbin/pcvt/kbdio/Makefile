# $FreeBSD: src/usr.sbin/pcvt/kbdio/Makefile,v 1.11.8.1 2005/02/13 07:23:45 obrien Exp $

PROG=	kbdio
NOMAN=
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
