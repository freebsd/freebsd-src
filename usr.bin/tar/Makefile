# Makefile for bsdtar
#
# $FreeBSD$
#
DEBUG_FLAGS= -g

PROG=	bsdtar
SRCS=	bsdtar.c matching.c read.c util.c write.c
MAN =   bsdtar.1

BINDIR?=	/usr/bin
WARNS?=	6
LDADD += -larchive -lz -lbz2

.if defined(DMALLOC)
CFLAGS += -DDMALLOC -I/usr/local/include
LDADD += -L/usr/local/lib -ldmalloc
.endif

.include <bsd.prog.mk>

