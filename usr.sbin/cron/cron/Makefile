# $FreeBSD: src/usr.sbin/cron/cron/Makefile,v 1.12 1999/08/28 01:15:49 peter Exp $

PROG=	cron
SRCS=	cron.c database.c do_command.c job.c user.c popen.c
CFLAGS+=-DLOGIN_CAP
DPADD=	${LIBCRON} ${LIBUTIL}
LDADD=	${LIBCRON} -lutil
MAN8=	cron.8

.include <bsd.prog.mk>
