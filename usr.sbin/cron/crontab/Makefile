# $FreeBSD$

BINDIR=	/usr/bin

PROG=	crontab
MAN=	crontab.1 crontab.5
BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

CFLAGS+= -I${.CURDIR}/../cron

DPADD=	${LIBCRON} ${LIBUTIL}
LDADD=	${LIBCRON} -lutil

.include <bsd.prog.mk>
