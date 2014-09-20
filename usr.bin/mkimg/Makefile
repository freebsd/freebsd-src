# $FreeBSD$

.include <src.opts.mk>

PROG=	mkimg
SRCS=	format.c image.c mkimg.c scheme.c
MAN=	mkimg.1

CFLAGS+=-DSPARSE_WRITE

# List of formats to support
SRCS+= \
	raw.c \
	vhd.c \
	vmdk.c

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

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
