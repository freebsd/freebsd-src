# $FreeBSD$

PROG=	ccdconfig

CFLAGS+=	-I${.CURDIR}/../../sys
LDADD+=		-lkvm
DPADD+=		${LIBKVM}
BINGRP=		kmem
BINMODE=	2555

.include <bsd.prog.mk>
