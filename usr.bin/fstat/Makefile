#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id$

PROG=	fstat
CFLAGS+=-I${.CURDIR}/../../sys
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
