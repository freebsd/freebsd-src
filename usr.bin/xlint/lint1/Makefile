#	$NetBSD: Makefile,v 1.3 1995/07/04 01:53:05 cgd Exp $
# $FreeBSD$

PROG=	lint1
SRCS=	cgram.y scan.l mem1.c mem.c err.c main1.c decl.c tree.c func.c \
	init.c emit.c emit1.c inittyp.c
MAN=	lint.7

LDADD+=	-ll -lm
DPADD+=	${LIBL} ${LIBM}
CFLAGS+= -I. -I${.CURDIR}
LINTFLAGS=-aehpz

BINDIR=	/usr/libexec

.include <bsd.prog.mk>

.PATH: ../common

lint.7:	makeman
	sh ${.CURDIR}/makeman ${.OBJDIR}/${PROG} -m >${.TARGET}
