#	@(#)Makefile	8.1 (Berkeley) 6/6/93

MAINTAINER=ken@FreeBSD.ORG

PROG=	iostat
CFLAGS+=-I${.CURDIR}/../../sys
SRCS=	iostat.c
MAN8=	iostat.8
DPADD=	${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lkvm -ldevstat
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
