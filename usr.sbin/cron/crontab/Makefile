BINDIR?=	/usr/bin

PROG=	crontab
SRCS=	crontab.c
CFLAGS+=-I${.CURDIR}/../cron
MAN1=	crontab.1
MAN5=	crontab.5

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

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
