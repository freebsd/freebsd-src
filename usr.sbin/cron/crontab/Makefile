# $FreeBSD: src/usr.sbin/cron/crontab/Makefile,v 1.14.18.1 2008/11/25 02:59:29 kensmith Exp $

BINDIR=	/usr/bin

PROG=	crontab
MAN=	crontab.1 crontab.5
BINOWN=	root
BINMODE=4555
PRECIOUSPROG=

CFLAGS+= -I${.CURDIR}/../cron

DPADD=	${LIBCRON} ${LIBMD} ${LIBUTIL}
LDADD=	${LIBCRON} -lmd -lutil

.include <bsd.prog.mk>
