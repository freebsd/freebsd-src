#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/syslogd/Makefile,v 1.3.6.2 2001/04/25 12:11:00 ru Exp $

PROG=	syslogd
SRCS=	syslogd.c ttymsg.c
.PATH:	${.CURDIR}/../../usr.bin/wall
MAN=	syslog.conf.5 syslogd.8
COPTS+=	-Wall -DINET6

.include <bsd.prog.mk>
