#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/iostat/Makefile,v 1.5.2.1 2001/04/25 12:09:40 ru Exp $

MAINTAINER=ken@FreeBSD.ORG

PROG=	iostat
MAN=	iostat.8
CFLAGS+=-I${.CURDIR}/../../sys
DPADD=	${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lkvm -ldevstat
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
