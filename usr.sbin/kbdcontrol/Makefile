
PACKAGE=	console-tools
PROG=	kbdcontrol
MAN=	kbdcontrol.1 kbdmap.5
MLINKS= kbdmap.5 keymap.5
SRCS=	kbdcontrol.c lex.l

WARNS?=	4
CFLAGS+= -I${.CURDIR}

# temporarily added for pre-Unicode accent key API/ABI compatibility
CFLAGS+= -DCOMPAT_FREEBSD13

.include <bsd.prog.mk>
