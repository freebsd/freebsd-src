# This is the Makefile for the libuucp subdirectory of Taylor UUCP
# $FreeBSD: src/gnu/libexec/uucp/libuucp/Makefile,v 1.6 1999/08/27 23:33:42 peter Exp $

LIB=		uucp
SRCS = buffer.c crc.c debug.c escape.c getopt.c getop1.c parse.c spool.c \
	status.c xfree.c xmall.c xreall.c \
	 getlin.c
CFLAGS+=	-I$(.CURDIR)/../common_sources

NOMAN=		noman
NOPROFILE=	noprofile

install:

.include <bsd.lib.mk>
