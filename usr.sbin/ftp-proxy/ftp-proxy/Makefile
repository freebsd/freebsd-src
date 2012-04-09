# $FreeBSD: src/usr.sbin/ftp-proxy/ftp-proxy/Makefile,v 1.1.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.PATH:	${.CURDIR}/../../../contrib/pf/ftp-proxy

PROG=	ftp-proxy
MAN=	ftp-proxy.8

SRCS=   ftp-proxy.c filter.c

CFLAGS+= -I${.CURDIR}/../../../contrib/pf/libevent
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/pf

DPADD=	${LIBEVENT}
LDADD=	${LIBEVENT}

WARNS?=	2

.include <bsd.prog.mk>
