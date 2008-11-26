# $FreeBSD: src/usr.sbin/pcvt/vgaio/Makefile,v 1.17.26.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	vgaio
MAN=	vgaio.8
SRCS=	vgaio.y lex.l y.tab.h

CFLAGS+= -I${.OBJDIR} -I${.CURDIR}

YACC=	yacc

#YFLAGS+= -yd	# Bison
#YFLAGS+= -v	# verbose

LFLAGS+= -I

DPADD=	${LIBM} ${LIBY} ${LIBL}
LDADD=	-lm -ly -ll

CLEANFILES+= y.output	# comment file from bison

install:
	@${ECHO} "vgaio is not installed automatically ...."

.include <bsd.prog.mk>
