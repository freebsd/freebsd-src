# $FreeBSD: src/usr.sbin/cron/cron/Makefile,v 1.11.2.1 1999/08/29 15:40:33 peter Exp $

PROG=	cron
SRCS=	cron.c database.c do_command.c job.c user.c popen.c
CFLAGS+=-DLOGIN_CAP
DPADD=	${LIBCRON} ${LIBUTIL}
LDADD=	${LIBCRON} -lutil
MAN8=	cron.8

.include <bsd.prog.mk>
