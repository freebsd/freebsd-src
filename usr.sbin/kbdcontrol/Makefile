# $FreeBSD: src/usr.sbin/kbdcontrol/Makefile,v 1.10 2002/07/11 21:12:18 alfred Exp $

PROG=	kbdcontrol
MAN=	kbdcontrol.1 kbdmap.5
MLINKS= kbdmap.5 keymap.5
SRCS=	kbdcontrol.c lex.l

CFLAGS+= -I${.CURDIR}

DPADD=	${LIBL}
LDADD=	-ll

WARNS?=	4

.include <bsd.prog.mk>
