PROG=	ccdcontrol
SRCS=	ccdcontrol.c
MAN8=	ccdcontrol.8

CFLAGS+=	-I${.CURDIR}/../../sys
LDADD+=		-lkvm
DPADD+=		${LIBKVM}
BINGRP=		kmem
BINMODE=	2555

.include <bsd.prog.mk>
