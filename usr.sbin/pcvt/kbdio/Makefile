# $FreeBSD: src/usr.sbin/pcvt/kbdio/Makefile,v 1.13.12.1 2008/10/02 02:57:24 kensmith Exp $

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
