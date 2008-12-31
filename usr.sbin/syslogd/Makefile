#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/syslogd/Makefile,v 1.13.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../usr.bin/wall

PROG=	syslogd
MAN=	syslog.conf.5 syslogd.8
SRCS=	syslogd.c ttymsg.c

DPADD=	${LIBUTIL}
LDADD=	-lutil

WARNS?=	1

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

CFLAGS+= -I${.CURDIR}/../../usr.bin/wall

.include <bsd.prog.mk>
