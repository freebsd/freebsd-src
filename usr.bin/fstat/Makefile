#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.2 1997/08/22 14:22:31 jmg Exp $

PROG=	fstat
CFLAGS+=-I${.CURDIR}/../../sys
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
