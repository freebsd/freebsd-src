BINDIR?=	/usr/sbin

PROG=	cron
SRCS=	cron.c database.c do_command.c job.c user.c popen.c
MAN3=	bitstring.3
MAN8=	cron.8

.if exists({.CURDIR}/../lib/obj)
LDADD+=	-L${.CURDIR}/../lib/obj -lcron
.else
LDADD+=	-L${.CURDIR}/../lib -lcron
.endif

.include <bsd.prog.mk>
