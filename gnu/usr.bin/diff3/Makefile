
DIFFSRC=${SRCTOP}/contrib/diff/src
.PATH:  ${DIFFSRC} \
	${SRCTOP}/contrib/diff/lib \
	${SRCTOP}/contrib/diff/man

PROG=		diff3
SRCS=           diff3.c version-etc.c \
		xmalloc.c error.c c-stack.c exitfail.c cmpbuf.c

# Important for ctype macros!
CFLAGS+=-funsigned-char

CFLAGS+=-I${SRCTOP}/contrib/diff/
CFLAGS+=-I${SRCTOP}/contrib/diff/src
CFLAGS+=-I${SRCTOP}/contrib/diff/lib
CFLAGS+=-DHAVE_CONFIG_H
CFLAGS+=-DDEFAULT_DIFF_PROGRAM=\"/usr/bin/diff\"

WARNS?=	0

.include <bsd.prog.mk>
