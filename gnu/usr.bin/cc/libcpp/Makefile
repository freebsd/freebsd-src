# $FreeBSD: src/gnu/usr.bin/cc/libcpp/Makefile,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $

# Use our headers in preference to ones from ../cc_tools.
CFLAGS+=	-I${.CURDIR} -I.

.include "../Makefile.inc"

.PATH: ${GCCLIB}/libcpp

LIB=	cpp
SRCS=	localedir.h
SRCS+=	charset.c directives.c errors.c expr.c files.c \
	identifiers.c init.c lex.c line-map.c macro.c mkdeps.c \
	pch.c symtab.c traditional.c
INTERNALLIB=
WARNS?=	1

localedir.h:	Makefile
	echo	'#define LOCALEDIR "/usr/share/locale"' > localedir.h
SRCS+=		localedir.h
CLEANFILES+=	localedir.h

.include <bsd.lib.mk>
