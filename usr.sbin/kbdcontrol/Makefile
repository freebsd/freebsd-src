# $FreeBSD: src/usr.sbin/kbdcontrol/Makefile,v 1.3.14.2 2001/04/25 12:09:45 ru Exp $

PROG=	kbdcontrol
SRCS=	kbdcontrol.c lex.l
MAN=	kbdcontrol.1 kbdmap.5
MLINKS= kbdmap.5 keymap.5
DPADD=	${LIBL}
LDADD=	-ll

.include <bsd.prog.mk>
