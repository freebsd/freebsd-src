# This is the Makefile for the libuucp subdirectory of Taylor UUCP
# $FreeBSD: src/gnu/libexec/uucp/libuucp/Makefile,v 1.6.2.1 2002/07/19 18:46:22 ru Exp $

LIB=		uucp
INTERNALLIB=	YES
SRCS = buffer.c crc.c debug.c escape.c getopt.c getop1.c parse.c spool.c \
	status.c xfree.c xmall.c xreall.c \
	 getlin.c
CFLAGS+=	-I$(.CURDIR)/../common_sources

.include <bsd.lib.mk>
