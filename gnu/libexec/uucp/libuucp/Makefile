# This is the Makefile for the libuucp subdirectory of Taylor UUCP
# $FreeBSD$

LIB=		uucp
INTERNALLIB=	YES
SRCS = buffer.c crc.c debug.c escape.c getopt.c getop1.c parse.c spool.c \
	status.c xfree.c xmall.c xreall.c \
	 getlin.c
CFLAGS+=	-I$(.CURDIR)/../common_sources

.include <bsd.lib.mk>
