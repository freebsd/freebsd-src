# $FreeBSD$

PROG=	kbdcontrol
MAN=	kbdcontrol.1 kbdmap.5
MLINKS= kbdmap.5 keymap.5
SRCS=	kbdcontrol.c lex.l

WARNS?=	4
CFLAGS+= -I${.CURDIR}

DPADD=	${LIBL}
LDADD=	-ll

.include <bsd.prog.mk>
