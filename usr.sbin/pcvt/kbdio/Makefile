# $FreeBSD: src/usr.sbin/pcvt/kbdio/Makefile,v 1.11 2001/07/20 06:20:08 obrien Exp $

PROG=	kbdio
NOMAN=	#true
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
