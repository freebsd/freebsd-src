# $FreeBSD: src/sbin/ccdconfig/Makefile,v 1.2.10.1 2001/04/25 10:58:12 ru Exp $

PROG=	ccdconfig
MAN=	ccdconfig.8

CFLAGS+=	-I${.CURDIR}/../../sys
LDADD+=		-lkvm
DPADD+=		${LIBKVM}
BINGRP=		kmem
BINMODE=	2555

.include <bsd.prog.mk>
