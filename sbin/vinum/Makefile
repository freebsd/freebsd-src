# $FreeBSD$

PROG=	vinum
SRCS=	v.c list.c vinumparser.c vinumutil.c vext.h commands.c
MAN=	vinum.8

CFLAGS+=	-I${.CURDIR}/../../sys -Wall -DVINUMDEBUG

DPADD=	${LIBREADLINE} ${LIBTERMCAP} ${LIBDEVSTAT} ${LIBKVM}
LDADD=	-lreadline -ltermcap -ldevstat -lkvm

.PATH: ${.CURDIR}/../../sys/dev/vinum

.include <bsd.prog.mk>
