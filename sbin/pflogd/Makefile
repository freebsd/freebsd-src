# $FreeBSD: src/sbin/pflogd/Makefile,v 1.2 2004/06/16 23:39:33 mlaier Exp $

.PATH:	${.CURDIR}/../../contrib/pf/pflogd

PROG=	pflogd
SRCS= 	pflogd.c pidfile.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

CFLAGS+=-Wall -Werror -Wmissing-prototypes -Wshadow
LDADD+= -lpcap -lutil
DPAPP+= ${LIBPCAP} ${LIBUTIL}

.include <bsd.prog.mk>
