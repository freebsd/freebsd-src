# This is the Makefile for the libuucp subdirectory of Taylor UUCP
# $Id: Makefile,v 1.2 1993/08/05 16:14:59 jtc Exp $

LIB=		uucp
SRCS=		buffer.o crc.o debug.o escape.o getopt.o getop1.o parse.o \
		spool.o status.o xfree.o xmall.o xreall.o getlin.o
CFLAGS+=	-I$(.CURDIR)/../common_sources

NOMAN=		noman
NOPROFILE=	noprofile

install:

.include <bsd.lib.mk>
