#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	vmstat
NO_WERROR=yes
MAN=	vmstat.8
BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lkvm -ldevstat

.include <bsd.prog.mk>
