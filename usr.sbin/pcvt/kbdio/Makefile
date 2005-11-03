# $FreeBSD: src/usr.sbin/pcvt/kbdio/Makefile,v 1.13 2004/12/21 08:47:34 ru Exp $

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
