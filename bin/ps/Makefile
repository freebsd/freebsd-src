# $FreeBSD: src/bin/ps/Makefile,v 1.23 2003/05/02 01:05:05 obrien Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/2/93

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c
#
# To support "lazy" ps for non root/wheel users
# add -DLAZY_PS to the cflags.  This helps
# keep ps from being an unnecessary load
# on large systems.
#
WARNS=	1
WFORMAT=0
CFLAGS+=-DLAZY_PS
DPADD=	${LIBM} ${LIBKVM}
LDADD=	-lm -lkvm
#BINGRP=	kmem
#BINMODE=2555

.include <bsd.prog.mk>
