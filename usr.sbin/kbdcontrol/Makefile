# $FreeBSD$

PROG=	kbdcontrol
SRCS=	kbdcontrol.c lex.l
CFLAGS+= -I${.CURDIR}
MAN=	kbdcontrol.1 kbdmap.5
MLINKS= kbdmap.5 keymap.5
DPADD=	${LIBL}
LDADD=	-ll

.include <bsd.prog.mk>
