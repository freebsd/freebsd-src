#	@(#)Makefile	8.1 (Berkeley) 6/2/93
#	$Id: Makefile,v 1.6 1995/10/25 15:07:38 torstenb Exp $

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c
CFLAGS+=-I${.CURDIR}/../../sys
DPADD=	${LIBM} ${LIBKVM}
LDADD=	-lm -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
