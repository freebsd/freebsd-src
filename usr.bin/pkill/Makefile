#	$NetBSD: Makefile,v 1.1 2002/03/01 11:21:58 ad Exp $
# $FreeBSD$

PROG=	pkill
WARNS?=	5

DPADD=	${LIBKVM}
LDADD=	-lkvm

#BINGRP=	kmem
#BINMODE=	2555

LINKS=	${BINDIR}/pkill ${BINDIR}/pgrep
MLINKS=	pkill.1 pgrep.1

.include <bsd.prog.mk>
