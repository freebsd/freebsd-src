# $FreeBSD$

PROG=	cron
SRCS=	cron.c database.c do_command.c job.c user.c popen.c
CFLAGS+=-DLOGIN_CAP
DPADD=	${LIBCRON} ${LIBUTIL}
LDADD=	${LIBCRON} -lutil

.include <bsd.prog.mk>
