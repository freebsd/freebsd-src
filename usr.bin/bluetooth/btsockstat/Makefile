# $Id: Makefile,v 1.7 2003/08/14 20:07:14 max Exp $
# $FreeBSD$

PROG=		btsockstat
MAN=		btsockstat.1
SRCS=		btsockstat.c
WARNS?=		2
BINGRP=		kmem
BINMODE=	2555

DPADD=		${LIBBLUETOOTH} ${LIBKVM}
LDADD=		-lbluetooth -lkvm

.include <bsd.prog.mk>
