# $FreeBSD: src/usr.sbin/kbdcontrol/Makefile,v 1.11.34.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	kbdcontrol
MAN=	kbdcontrol.1 kbdmap.5
MLINKS= kbdmap.5 keymap.5
SRCS=	kbdcontrol.c lex.l

WARNS?=	4
CFLAGS+= -I${.CURDIR}

DPADD=	${LIBL}
LDADD=	-ll

.include <bsd.prog.mk>
