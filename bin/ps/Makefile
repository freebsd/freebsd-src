#	@(#)Makefile	8.1 (Berkeley) 6/2/93
#	$Id: Makefile,v 1.3 1994/09/24 02:56:40 davidg Exp $

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c
CFLAGS+=-I/sys
DPADD=	${LIBMATH} ${LIBKVM}
LDADD=	-lm -lkvm
BINOWN= root
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
