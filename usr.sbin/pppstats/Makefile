# $FreeBSD$

PROG=	pppstats
MAN8=	pppstats.8

DPADD=	${LIBUTIL} ${LIBKVM}
LDADD=	-lutil -lkvm

BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
