# $FreeBSD: src/usr.sbin/cron/cron/Makefile,v 1.17.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	cron
MAN=	cron.8
SRCS=	cron.c database.c do_command.c job.c user.c popen.c

CFLAGS+= -DLOGIN_CAP -DPAM

DPADD=	${LIBCRON} ${LIBPAM} ${LIBUTIL}
LDADD=	${LIBCRON} ${MINUSLPAM} -lutil

.include <bsd.prog.mk>
