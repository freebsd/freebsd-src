# $FreeBSD$

.PATH:	${.CURDIR}/../../../../sbin/dhclient

SRCS=	alloc.c convert.c hash.c options.c tables.c			\
	fake.c								\
	option-domain-search.c

CFLAGS+= -I${.CURDIR}/../../../../sbin/dhclient
LDADD=	-lutil

PROG=	option-domain-search

NO_MAN=
WARNS?=	2

.include <bsd.prog.mk>
