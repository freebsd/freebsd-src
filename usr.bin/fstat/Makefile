#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.1.1.1.8.1 1997/08/25 12:52:30 jmg Exp $

PROG=	fstat
CFLAGS+=-I${.CURDIR}/../../sys
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
