PROG=	crontab
SRCS=	crontab.c misc.c entry.c env.c
CFLAGS+= -I${.CURDIR}/../../usr.sbin/cron
BINOWN =root
BINMODE=4111
MAN1=	crontab.1
MAN5=	crontab.5

.include <bsd.prog.mk>
.PATH: ${.CURDIR}/../../usr.sbin/cron
