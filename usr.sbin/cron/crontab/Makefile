BINDIR?=	/usr/bin

PROG=	crontab
SRCS=	crontab.c
CFLAGS+= -I${.CURDIR}/../cron
MAN1=	crontab.1
MAN5=	crontab.5

.if exists(${.OBJDIR}/../lib)
LDDESTDIR+=	-L${.OBJDIR}/../lib
DPADD+=	${.OBJDIR}/../lib/libcron.a
.else
LDDESTDIR+=	-L${.CURDIR}/../lib
DPADD+=	${.CURDIR}/../lib/libcron.a
.endif

LDADD+=	-lcron

BINOWN= root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
