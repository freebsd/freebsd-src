#	$Id: Makefile,v 1.11 1998/03/07 09:47:48 bde Exp $

PROG=	cron
SRCS=	cron.c database.c do_command.c job.c user.c popen.c
CFLAGS+=-DLOGIN_CAP
DPADD=	${LIBCRON} ${LIBUTIL}
LDADD=	${LIBCRON} -lutil
MAN8=	cron.8

.include <bsd.prog.mk>
