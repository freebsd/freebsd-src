# $FreeBSD: src/sbin/pflogd/Makefile,v 1.4.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH:	${.CURDIR}/../../contrib/pf/pflogd

PROG=	pflogd
SRCS= 	pflogd.c pidfile.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

LDADD=	-lpcap -lutil
DPADD=	${LIBPCAP} ${LIBUTIL}

WARNS?=	2

.include <bsd.prog.mk>
