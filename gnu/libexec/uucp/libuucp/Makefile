# This is the Makefile for the libuucp subdirectory of Taylor UUCP
# $FreeBSD: src/gnu/libexec/uucp/libuucp/Makefile,v 1.5.2.1 1999/08/29 14:30:20 peter Exp $

LIB=		uucp
SRCS = buffer.c crc.c debug.c escape.c getopt.c getop1.c parse.c spool.c \
	status.c xfree.c xmall.c xreall.c \
	 getlin.c
CFLAGS+=	-I$(.CURDIR)/../common_sources

NOMAN=		noman
NOPROFILE=	noprofile

install:

.include <bsd.lib.mk>
