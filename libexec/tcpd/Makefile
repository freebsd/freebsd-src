# $FreeBSD: src/libexec/tcpd/Makefile,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/tcp_wrappers

PROG=	tcpd
MAN=	tcpd.8
CSTD?=	c89
CFLAGS+=-DREAL_DAEMON_DIR=\"/usr/libexec\" \
	-DSEVERITY=LOG_INFO -DRFC931_TIMEOUT=10 \
	-DHOSTS_DENY=\"/etc/hosts.deny\" -DHOSTS_ALLOW=\"/etc/hosts.allow\" \
	-DFACILITY=LOG_DAEMON
.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=-DINET6
.endif

DPADD=	${LIBWRAP}
LDADD=	-lwrap

.include <bsd.prog.mk>
