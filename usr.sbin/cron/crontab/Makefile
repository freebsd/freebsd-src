# $FreeBSD$

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
