BINDIR?=	/usr/bin

PROG=	crontab
SRCS=	crontab.c 
CFLAGS+= -I${.CURDIR}/../cron
LDADD+=	-L${.CURDIR}/../lib -lcron
MAN1=	crontab.1
MAN5=	crontab.5

.include <bsd.prog.mk>
