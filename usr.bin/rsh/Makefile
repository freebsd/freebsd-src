#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/rsh/Makefile,v 1.22 2002/10/16 16:10:46 markm Exp $

PROG=	rsh
SRCS=	rsh.c
CFLAGS+=-I${.CURDIR}/../../libexec/rlogind

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
