BINDIR?=	/usr/bin

PROG=	crontab
SRCS=	crontab.c 
CFLAGS+= -I${.CURDIR}/../cron
MAN1=	crontab.1
MAN5=	crontab.5

.if exists(${.CURDIR}/../lib/obj)
LDADD+=	-L${.CURDIR}/../lib/obj -lcron
.else
LDADD+=	-L${.CURDIR}/../lib -lcron
.endif

.include <bsd.prog.mk>
