# $FreeBSD: src/usr.sbin/cron/crontab/Makefile,v 1.12.8.1 2005/08/08 12:50:57 dds Exp $

BINDIR=	/usr/bin

PROG=	crontab
MAN=	crontab.1 crontab.5
BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

CFLAGS+= -I${.CURDIR}/../cron

DPADD=	${LIBCRON} ${LIBMD} ${LIBUTIL}
LDADD=	${LIBCRON} -lmd -lutil

.include <bsd.prog.mk>
