# $FreeBSD: src/bin/ps/Makefile,v 1.13 1999/11/17 13:37:30 peter Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/2/93

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c
#
# To support "lazy" ps for non root/wheel users
# add -DLAZY_PS to the cflags.  This helps
# keep ps from being an unnecessary load
# on large systems.
#
CFLAGS+=-I${.CURDIR}/../../sys -DLAZY_PS
DPADD=	${LIBM} ${LIBKVM}
LDADD=	-lm -lkvm
#BINGRP=	kmem
#BINMODE=2555

.include <bsd.prog.mk>
