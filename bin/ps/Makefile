#	@(#)Makefile	8.1 (Berkeley) 6/2/93
#	$Id: Makefile,v 1.5 1995/04/15 03:31:19 davidg Exp $

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c
CFLAGS+=-I/sys
DPADD=	${LIBMATH} ${LIBKVM}
LDADD=	-lm -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
