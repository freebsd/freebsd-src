# $FreeBSD: src/usr.sbin/tcpdmatch/Makefile,v 1.10.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/tcp_wrappers

PROG=	tcpdmatch
MAN=	tcpdmatch.8
SRCS=	tcpdmatch.c fakelog.c inetcf.c scaffold.c

CFLAGS+=-DREAL_DAEMON_DIR=\"/usr/libexec\" \
	-DSEVERITY=LOG_INFO -DRFC931_TIMEOUT=10
.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=-DINET6
.endif

DPADD=	${LIBWRAP}
LDADD=	-lwrap

.include <bsd.prog.mk>
