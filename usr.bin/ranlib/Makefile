#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/ranlib/Makefile,v 1.8 2001/03/27 10:52:05 ru Exp $

PROG=	ranlib
SRCS=	archive.c build.c misc.c ranlib.c touch.c
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../ar
MAN=	ranlib.1aout ranlib.5
BINDIR= /usr/libexec/aout
VPATH=	${.CURDIR}/../ar

.include <bsd.prog.mk>
