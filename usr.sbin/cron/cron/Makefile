# $FreeBSD$

PROG=	cron
MAN=	cron.8
SRCS=	cron.c database.c do_command.c job.c user.c popen.c

CFLAGS+= -DLOGIN_CAP -DPAM

DPADD=	${LIBCRON} ${LIBPAM} ${LIBUTIL}
LDADD=	${LIBCRON} -lpam -lutil

.include <bsd.prog.mk>
