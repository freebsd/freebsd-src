# $FreeBSD: src/usr.sbin/tcpdchk/Makefile,v 1.11.12.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/tcp_wrappers

PROG=	tcpdchk
MAN=	tcpdchk.8
SRCS=	tcpdchk.c fakelog.c inetcf.c scaffold.c

CFLAGS+=-DREAL_DAEMON_DIR=\"/usr/libexec\" \
	-DSEVERITY=LOG_INFO -DRFC931_TIMEOUT=10 -DPROCESS_OPTIONS \
	-DHOSTS_DENY=\"/etc/hosts.deny\" -DHOSTS_ALLOW=\"/etc/hosts.allow\"
.if !defined(NO_INET6)
CFLAGS+=-DINET6
.endif

DPADD=	${LIBWRAP}
LDADD=	-lwrap

.include <bsd.prog.mk>
