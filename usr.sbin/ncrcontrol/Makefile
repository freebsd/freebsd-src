PROG=	ncrcontrol
SRCS=	ncrcontrol.c
MAN8=	ncrcontrol.8

CFLAGS+=	-I${.CURDIR}/../../sys
LDADD+=		-lkvm
DPADD+=		${LIBKVM}
BINGRP=		kmem
BINMODE=	2555

.include <bsd.prog.mk>
