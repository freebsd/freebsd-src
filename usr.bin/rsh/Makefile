#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

PROG=	rsh
SRCS=	rsh.c
CFLAGS+=-I${.CURDIR}/../../libexec/rlogind

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
