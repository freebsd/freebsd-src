BINDIR?=	/usr/sbin

PROG=	cron
SRCS=	cron.c database.c do_command.c job.c user.c popen.c
MAN8=	cron.8

.if exists(${.CURDIR}/../lib/obj)
LDDESTDIR+=	-L${.CURDIR}/../lib/obj
DPADD+=	${.CURDIR}/../lib/obj/libcron.a
.else
LDDESTDIR+=	-L${.CURDIR}/../lib
DPADD+=	${.CURDIR}/../lib/libcron.a
.endif

LDADD+= -lcron

.include <bsd.prog.mk>
