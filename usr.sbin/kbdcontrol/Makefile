# $FreeBSD: src/usr.sbin/kbdcontrol/Makefile,v 1.11.36.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	kbdcontrol
MAN=	kbdcontrol.1 kbdmap.5
MLINKS= kbdmap.5 keymap.5
SRCS=	kbdcontrol.c lex.l

WARNS?=	4
CFLAGS+= -I${.CURDIR}

DPADD=	${LIBL}
LDADD=	-ll

.include <bsd.prog.mk>
