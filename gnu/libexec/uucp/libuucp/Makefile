# This is the Makefile for the libuucp subdirectory of Taylor UUCP
# $Id: Makefile,v 1.3 1994/05/07 18:13:29 ache Exp $

LIB=		uucp
SRCS = buffer.c crc.c debug.c escape.c getopt.c getop1.c parse.c spool.c \
	status.c xfree.c xmall.c xreall.c \
	 getlin.c
CFLAGS+=	-I$(.CURDIR)/../common_sources

NOMAN=		noman
NOPROFILE=	noprofile

install:

.include <bsd.lib.mk>
