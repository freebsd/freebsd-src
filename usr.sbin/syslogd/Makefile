#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/syslogd/Makefile,v 1.3.6.4 2003/05/19 16:23:03 gshapiro Exp $

.PATH: ${.CURDIR}/../../usr.bin/wall

PROG=	syslogd
MAN=	syslog.conf.5 syslogd.8
SRCS=	syslogd.c ttymsg.c

CFLAGS+=-DINET6 -I${.CURDIR}/../../usr.bin/wall

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

WARNS?=	1

.include <bsd.prog.mk>
