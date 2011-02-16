# $FreeBSD: src/libexec/tcpd/Makefile,v 1.6.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

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
