# $FreeBSD: src/usr.sbin/cron/crontab/Makefile,v 1.10.2.1 2001/04/25 12:09:24 ru Exp $

PROG=	crontab
CFLAGS+=-I${.CURDIR}/../cron
DPADD=	${LIBCRON} ${LIBUTIL}
LDADD=	${LIBCRON} -lutil
MAN=	crontab.1 crontab.5

BINDIR=	/usr/bin
BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
