# $FreeBSD: src/usr.sbin/kbdcontrol/Makefile,v 1.3.14.1 2000/10/29 17:20:32 dwmalone Exp $

PROG=	kbdcontrol
SRCS=	kbdcontrol.c lex.l
MAN1=	kbdcontrol.1
MAN5=	kbdmap.5
MLINKS= kbdmap.5 keymap.5
DPADD=	${LIBL}
LDADD=	-ll

.include <bsd.prog.mk>
