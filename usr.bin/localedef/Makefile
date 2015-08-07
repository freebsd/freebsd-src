# $FreeBSD$

PROG=	localedef
SRCS=	charmap.c \
	collate.c \
	ctype.c \
	localedef.c \
	messages.c \
	monetary.c \
	numeric.c \
	parser.y \
	scanner.c \
	time.c \
	wide.c

WARNS=	3
${SRCS:M*.c}: parser.h
parser.h: parser.y
LIBADD=	avl

IGNORE_PRAGMA=	yes

CFLAGS+=	-DNEED_SOLARIS_BOOLEAN
CFLAGS+=	-I. -I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../lib/libc/locale
CFLAGS+=	-I${.CURDIR}/../../lib/libc/stdtime
CFLAGS+=	-I${.CURDIR}/../../sys/cddl/compat/opensolaris
CFLAGS+=	-I${.CURDIR}/../../sys/cddl/contrib/opensolaris/uts/common

.include <bsd.prog.mk>
