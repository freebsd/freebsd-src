#	$Id: Makefile,v 1.1.1.1 1994/11/12 06:07:15 lars Exp $

PROG=	pppstats
MAN8=	pppstats.8

DPADD=	${LIBUTIL} ${LIBKVM}
LDADD=	-lutil -lkvm

BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
