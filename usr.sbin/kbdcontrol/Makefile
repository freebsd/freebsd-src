# $FreeBSD$

PROG=	kbdcontrol
MAN=	kbdcontrol.1 kbdmap.5
MLINKS= kbdmap.5 keymap.5
SRCS=	kbdcontrol.c lex.l

CFLAGS+= -I${.CURDIR}

DPADD=	${LIBL}
LDADD=	-ll

WARNS?=	4

.include <bsd.prog.mk>
