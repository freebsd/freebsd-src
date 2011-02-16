# $FreeBSD: src/sbin/pflogd/Makefile,v 1.3.24.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/../../contrib/pf/pflogd

PROG=	pflogd
SRCS= 	pflogd.c pidfile.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

CFLAGS+=-Wall -Werror -Wmissing-prototypes -Wshadow
LDADD=	-lpcap -lutil
DPADD=	${LIBPCAP} ${LIBUTIL}

.include <bsd.prog.mk>
