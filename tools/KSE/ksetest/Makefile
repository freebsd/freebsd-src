# $FreeBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/2/93

PROG=	ksetest
SRCS=	kse_threads_test.c kse_asm.S
#
# To support "lazy" ps for non root/wheel users
# add -DLAZY_PS to the cflags.  This helps
# keep ps from being an unnecessary load
# on large systems.
#
CFLAGS=-static -g -O
#WARNS=	0
#DPADD=	${LIBM} ${LIBKVM}
#LDADD=	-lm -lkvm
#BINGRP=	kmem
#BINMODE=2555
NOMAN=yes

.include <bsd.prog.mk>
