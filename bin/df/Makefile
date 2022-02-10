#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD$

PACKAGE=runtime

PROG=	df
SRCS=	df.c

CFLAGS+= -DMOUNT_CHAR_DEVS

LIBADD=	xo util

.include <bsd.prog.mk>
