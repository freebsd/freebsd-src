#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/syslogd/Makefile,v 1.9 2001/09/09 14:30:36 dd Exp $

.PATH: ${.CURDIR}/../../usr.bin/wall

PROG=	syslogd
MAN=	syslog.conf.5 syslogd.8
SRCS=	syslogd.c ttymsg.c

CFLAGS+=-DINET6 -I${.CURDIR}/../../usr.bin/wall
WARNS?=	1

.include <bsd.prog.mk>
