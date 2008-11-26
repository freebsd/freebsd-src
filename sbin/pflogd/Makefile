# $FreeBSD: src/sbin/pflogd/Makefile,v 1.3.16.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:	${.CURDIR}/../../contrib/pf/pflogd

PROG=	pflogd
SRCS= 	pflogd.c pidfile.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

CFLAGS+=-Wall -Werror -Wmissing-prototypes -Wshadow
LDADD=	-lpcap -lutil
DPADD=	${LIBPCAP} ${LIBUTIL}

.include <bsd.prog.mk>
