BINDIR?=	/usr/sbin

PROG=	cron
SRCS=	cron.c database.c do_command.c job.c user.c popen.c
MAN3=	bitstring.3
MAN8=	cron.8

LDADD+=	-L${.CURDIR}/../lib -lcron

.include <bsd.prog.mk>
