# $FreeBSD: src/sbin/vinum/Makefile,v 1.25 2003/05/01 07:19:29 grog Exp $

PROG=	vinum
SRCS=	v.c list.c vinumparser.c vinumutil.c vext.h commands.c
MAN=	vinum.8

CFLAGS+=	-I${.CURDIR}/../../sys
WARNS=	0

DPADD=	${LIBREADLINE} ${LIBTERMCAP} ${LIBDEVSTAT} ${LIBKVM}
LDADD=	-lreadline -ltermcap -ldevstat -lkvm

.PATH: ${.CURDIR}/../../sys/dev/vinum

.include <bsd.prog.mk>
