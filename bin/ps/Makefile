#	@(#)Makefile	8.1 (Berkeley) 6/2/93
#	$Id: Makefile,v 1.5.4.2 1996/06/05 02:36:06 jkh Exp $

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c
CFLAGS+=-I/sys
DPADD=	${LIBMATH} ${LIBKVM}
LDADD=	-lm -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
