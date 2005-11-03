# $FreeBSD: src/libexec/tcpd/Makefile,v 1.4 2004/12/21 10:49:26 ru Exp $

PROG=	tcpd
MAN=	tcpd.8
CSTD?=	c89
CFLAGS+=-DREAL_DAEMON_DIR=\"/usr/libexec\" \
	-DSEVERITY=LOG_INFO -DRFC931_TIMEOUT=10 \
	-DHOSTS_DENY=\"/etc/hosts.deny\" -DHOSTS_ALLOW=\"/etc/hosts.allow\" \
	-DFACILITY=LOG_DAEMON
.if !defined(NO_INET6)
CFLAGS+=-DINET6
.endif

DPADD=	${LIBWRAP}
LDADD=	-lwrap

.include <bsd.prog.mk>

.PATH:	${.CURDIR}/../../contrib/tcp_wrappers
