#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.PATH: ${.CURDIR}/../../usr.bin/wall

PROG=	syslogd
MAN=	syslog.conf.5 syslogd.8
SRCS=	syslogd.c ttymsg.c

CFLAGS+=-DINET6 -I${.CURDIR}/../../usr.bin/wall
WARNS?=	1

.include <bsd.prog.mk>
