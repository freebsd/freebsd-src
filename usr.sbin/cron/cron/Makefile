BINDIR?=	/usr/sbin

PROG=	cron
SRCS=	cron.c database.c do_command.c job.c user.c popen.c
MAN8=	cron.8

CFLAGS+=-DLOGIN_CAP
LDADD+= -lutil
DPADD+= ${LIBUTIL}

.if exists(${.OBJDIR}/../lib)
LDDESTDIR+=	-L${.OBJDIR}/../lib
DPADD+=	${.OBJDIR}/../lib/libcron.a
.else
LDDESTDIR+=	-L${.CURDIR}/../lib
DPADD+=	${.CURDIR}/../lib/libcron.a
.endif

LDADD+= -lcron

.include <bsd.prog.mk>
