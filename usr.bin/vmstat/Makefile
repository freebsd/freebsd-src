#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/vmstat/Makefile,v 1.9 2002/02/08 23:07:36 markm Exp $

PROG=	vmstat
MAN=	vmstat.8
BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lkvm -ldevstat

.include <bsd.prog.mk>
