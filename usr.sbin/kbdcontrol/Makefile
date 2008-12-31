# $FreeBSD: src/usr.sbin/kbdcontrol/Makefile,v 1.11.30.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	kbdcontrol
MAN=	kbdcontrol.1 kbdmap.5
MLINKS= kbdmap.5 keymap.5
SRCS=	kbdcontrol.c lex.l

WARNS?=	4
CFLAGS+= -I${.CURDIR}

DPADD=	${LIBL}
LDADD=	-ll

.include <bsd.prog.mk>
