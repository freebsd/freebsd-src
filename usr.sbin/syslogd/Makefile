#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	syslogd
SRCS=	syslogd.c ttymsg.c
.PATH:	${.CURDIR}/../../usr.bin/wall
MAN5=	syslog.conf.5
MAN8=	syslogd.8
COPTS+=	-Wall -DINET6

.include <bsd.prog.mk>
