BINDIR?=	/usr/sbin

PROG=	cron
SRCS=	cron.c database.c do_command.c job.c user.c popen.c
MAN8=	cron.8

CFLAGS+=-DLOGIN_CAP

.if exists(${.OBJDIR}/../lib)
LDADD+=	-L${.OBJDIR}/../lib
DPADD+=	${.OBJDIR}/../lib/libcron.a
.else
LDADD+=	-L${.CURDIR}/../lib
DPADD+=	${.CURDIR}/../lib/libcron.a
.endif
LDADD+=	-lcron

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
