#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	syslogd
SRCS=	syslogd.c ttymsg.c
.PATH:	${.CURDIR}/../../usr.bin/wall
MAN=	syslog.conf.5 syslogd.8
COPTS+=	-DINET6

.include <bsd.prog.mk>
