# $FreeBSD: src/sbin/vinum/Makefile,v 1.26 2004/02/23 20:13:57 johan Exp $

PROG=	vinum
SRCS=	v.c list.c vinumparser.c vinumutil.c vext.h commands.c
MAN=	vinum.8

CFLAGS+=	-I${.CURDIR}/../../sys
WARNS?=	0

DPADD=	${LIBREADLINE} ${LIBTERMCAP} ${LIBDEVSTAT} ${LIBKVM}
LDADD=	-lreadline -ltermcap -ldevstat -lkvm

.PATH: ${.CURDIR}/../../sys/dev/vinum

.include <bsd.prog.mk>
