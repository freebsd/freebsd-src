#	$Id$

PROG=	vinum
SRCS=	v.c list.c parser.c util.c vext.h commands.c
MAN8=	vinum.8

CFLAGS+=	-I${.CURDIR}/../../lkm/vinum -g -Wall -DDEBUG -DRAID5
DPADD=		${LIBUTIL} ${LIBREADLINE} ${LIBTERMCAP}
LDADD=		-lutil -lreadline -ltermcap
BINGRP=		kmem
BINMODE=	2555

.PATH: ${.CURDIR}/../../lkm/vinum

.include <bsd.prog.mk>
