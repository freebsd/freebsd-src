# $FreeBSD: src/usr.sbin/tcpdmatch/Makefile,v 1.10.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

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
