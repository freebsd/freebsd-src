# $FreeBSD$

PROG=	mkimg
SRCS=	mkimg.c scheme.c
MAN=	mkimg.1

CFLAGS+=-DSPARSE_WRITE

# List of schemes to support
SRCS+=	\
	apm.c \
	bsd.c \
	ebr.c \
	gpt.c \
	mbr.c \
	pc98.c \
	vtoc8.c

BINDIR?=/usr/bin

DPADD=	${LIBUTIL}
LDADD=	-lutil

WARNS?=	6

.include <bsd.prog.mk>
