#	$Id: Makefile,v 1.2 1998/10/17 13:48:37 bde Exp $

PROG=	vinum
SRCS=	v.c list.c parser.c util.c vext.h commands.c
MAN8=	vinum.8

CFLAGS+=	-I${.CURDIR}/../../sys/dev/vinum -g -Wall -DDEBUG -DRAID5
DPADD=		${LIBUTIL} ${LIBREADLINE} ${LIBTERMCAP}
LDADD=		-lutil -lreadline -ltermcap
BINGRP=		kmem
BINMODE=	2555

.PATH: ${.CURDIR}/../../sys/dev/vinum

.include <bsd.prog.mk>
