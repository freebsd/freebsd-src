#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

MAINTAINER=ken@FreeBSD.ORG

PROG=	iostat
MAN=	iostat.8
CFLAGS+=-I${.CURDIR}/../../sys
DPADD=	${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lkvm -ldevstat
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
