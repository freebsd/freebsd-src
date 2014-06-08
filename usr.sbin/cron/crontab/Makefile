# $FreeBSD$

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

NO_PIE=	yes

.include <bsd.prog.mk>
