# $FreeBSD: src/usr.sbin/cron/cron/Makefile,v 1.17.12.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	cron
MAN=	cron.8
SRCS=	cron.c database.c do_command.c job.c user.c popen.c

CFLAGS+= -DLOGIN_CAP -DPAM

DPADD=	${LIBCRON} ${LIBPAM} ${LIBUTIL}
LDADD=	${LIBCRON} ${MINUSLPAM} -lutil

.include <bsd.prog.mk>
