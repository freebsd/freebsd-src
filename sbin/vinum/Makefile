# $FreeBSD$

PROG=	vinum
SRCS=	v.c list.c vinumparser.c vinumutil.c vext.h commands.c
MAN8=	vinum.8

CFLAGS+=	-I${.CURDIR}/../../sys -g -Wall -DVINUMDEBUG
DPADD=		${LIBUTIL} ${LIBREADLINE} ${LIBTERMCAP}
LDADD=		-lutil -lreadline -ltermcap
BINGRP=		kmem
BINMODE=	555

.PATH: ${.CURDIR}/../../sys/dev/vinum

.include <bsd.prog.mk>

CFLAGS += -I ${.CURDIR}/../../sys/dev/vinum
