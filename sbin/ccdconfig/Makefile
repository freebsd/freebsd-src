PROG=	ccdconfig
SRCS=	ccdconfig.c
MAN8=	ccdconfig.8

CFLAGS+=	-I${.CURDIR}/../../sys
LDADD+=		-lkvm
DPADD+=		${LIBKVM}
BINGRP=		kmem
BINMODE=	2555

.include <bsd.prog.mk>
