#	Makefile,v 1.2 1994/11/14 08:02:40 bde Exp

PROG=	pppstats
MAN8=	pppstats.8

DPADD=	${LIBUTIL} ${LIBKVM}
LDADD=	-lutil -lkvm

BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
