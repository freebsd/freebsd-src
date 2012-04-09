# $FreeBSD: src/tools/regression/sbin/dhclient/Makefile,v 1.1.4.2.2.1 2012/03/03 06:15:13 kensmith Exp $

.PATH:	${.CURDIR}/../../../../sbin/dhclient

SRCS=	alloc.c convert.c hash.c options.c tables.c			\
	fake.c								\
	option-domain-search.c

CFLAGS+= -I${.CURDIR}/../../../../sbin/dhclient
LDADD=	-lutil

PROG=	option-domain-search

WARNS?=	2

.include <bsd.prog.mk>
