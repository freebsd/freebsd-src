# $FreeBSD: src/usr.sbin/pcvt/vgaio/Makefile,v 1.12 1999/08/28 01:17:50 peter Exp $

.if !exists(${.CURDIR}/../Makefile.inc)
error:
	@echo
	@echo "                      You MUST link/copy"
	@echo
	@echo "       ../Makefile.inc.NetBSD    or   ../Makefile.inc.FreeBSD"
	@echo
	@echo "             to ../Makefile.inc before you can proceed !"
	@echo
.else

PROG=	vgaio
MAN8=	vgaio.${MAN8EXT}

SRCS=	vgaio.y lex.l y.tab.h
YACC=	yacc

#YFLAGS+= -yd	# Bison
#YFLAGS+= -v	# verbose

LFLAGS+= -I

DPADD=	${LIBM} ${LIBY} ${LIBL}
LDADD=	-lm -ly -ll

CFLAGS=	-O2	# due to a gcc bug, it compiles only with -O2!
CFLAGS+= -I${.OBJDIR} -I${.CURDIR}

CLEANFILES+= y.output	# comment file from bison

install:
	@${ECHO} "vgaio is not installed automatically ...."

.include <bsd.prog.mk>

.endif
