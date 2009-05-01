# $FreeBSD: src/sbin/pflogd/Makefile,v 1.3.20.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/../../contrib/pf/pflogd

PROG=	pflogd
SRCS= 	pflogd.c pidfile.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

CFLAGS+=-Wall -Werror -Wmissing-prototypes -Wshadow
LDADD=	-lpcap -lutil
DPADD=	${LIBPCAP} ${LIBUTIL}

.include <bsd.prog.mk>
