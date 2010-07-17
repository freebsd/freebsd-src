# $FreeBSD: src/usr.sbin/cron/crontab/Makefile,v 1.16.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

BINDIR=	/usr/bin

PROG=	crontab
MAN=	crontab.1 crontab.5
BINOWN=	root
BINMODE=4555
PRECIOUSPROG=

WARNS?=	3

CFLAGS+= -I${.CURDIR}/../cron

DPADD=	${LIBCRON} ${LIBMD} ${LIBUTIL}
LDADD=	${LIBCRON} -lmd -lutil

.include <bsd.prog.mk>
