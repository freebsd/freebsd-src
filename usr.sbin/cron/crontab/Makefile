BINDIR?=	/usr/bin

PROG=	crontab
SRCS=	crontab.c 
CFLAGS+= -I${.CURDIR}/../cron
MAN1=	crontab.1
MAN5=	crontab.5

.if exists(${.CURDIR}/../lib/obj)
LDDESTDIR+=	-L${.CURDIR}/../lib/obj
DPADD+=	${.CURDIR}/../lib/obj/libcron.a
.else
LDDESTDIR+=	-L${.CURDIR}/../lib
DPADD+=	${.CURDIR}/../lib/libcron.a
.endif

LDADD+=	-lcron

.include <bsd.prog.mk>
