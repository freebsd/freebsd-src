# $FreeBSD: src/libexec/tcpd/Makefile,v 1.1.2.1 2002/08/31 18:43:41 dwmalone Exp $

MAINTAINER=	markm@FreeBSD.org

PROG=	tcpd
MAN=	tcpd.8
CFLAGS+=-DREAL_DAEMON_DIR=\"/usr/libexec\" \
	-DSEVERITY=LOG_INFO -DRFC931_TIMEOUT=10 \
	-DHOSTS_DENY=\"/etc/hosts.deny\" -DHOSTS_ALLOW=\"/etc/hosts.allow\" \
	-DFACILITY=LOG_DAEMON
.if !defined(NOINET6)
CFLAGS+=-DINET6
.endif

DPADD=	${LIBWRAP}
LDADD=	-lwrap

.include <bsd.prog.mk>

.PATH:	${.CURDIR}/../../contrib/tcp_wrappers
