# $FreeBSD: src/sbin/vinum/Makefile,v 1.24 2001/12/04 02:19:58 obrien Exp $

PROG=	vinum
SRCS=	v.c list.c vinumparser.c vinumutil.c vext.h commands.c
MAN=	vinum.8

CFLAGS+=	-I${.CURDIR}/../../sys -DVINUMDEBUG
WARNS=	0

DPADD=	${LIBREADLINE} ${LIBTERMCAP} ${LIBDEVSTAT} ${LIBKVM}
LDADD=	-lreadline -ltermcap -ldevstat -lkvm

.PATH: ${.CURDIR}/../../sys/dev/vinum

.include <bsd.prog.mk>
