#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/syslogd/Makefile,v 1.3.6.1 2001/02/18 16:22:59 ume Exp $

PROG=	syslogd
SRCS=	syslogd.c ttymsg.c
.PATH:	${.CURDIR}/../../usr.bin/wall
MAN5=	syslog.conf.5
MAN8=	syslogd.8
COPTS+=	-Wall -DINET6

.include <bsd.prog.mk>
