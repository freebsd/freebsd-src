# $FreeBSD: src/usr.sbin/kbdcontrol/Makefile,v 1.11.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	kbdcontrol
MAN=	kbdcontrol.1 kbdmap.5
MLINKS= kbdmap.5 keymap.5
SRCS=	kbdcontrol.c lex.l

WARNS?=	4
CFLAGS+= -I${.CURDIR}

DPADD=	${LIBL}
LDADD=	-ll

.include <bsd.prog.mk>
