#	$Id$

PROG=	crontab
CFLAGS+=-I${.CURDIR}/../cron
DPADD=	${LIBCRON} ${LIBUTIL}
LDADD=	${LIBCRON} -lutil
MAN1=	crontab.1
MAN5=	crontab.5

BINDIR=	/usr/bin
BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
