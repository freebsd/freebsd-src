#
# $FreeBSD: src/usr.sbin/tcpdchk/Makefile,v 1.3.2.1 2000/07/18 08:34:57 ume Exp $
#

MAINTAINER=markm@FreeBSD.org

PROG=	tcpdchk
MAN8=	tcpdchk.8
SRCS=	tcpdchk.c fakelog.c inetcf.c scaffold.c

CFLAGS=	-DREAL_DAEMON_DIR=\"/usr/libexec\" \
	-DSEVERITY=LOG_INFO -DRFC931_TIMEOUT=10 \
	-DHOSTS_DENY=\"/etc/hosts.deny\" -DHOSTS_ALLOW=\"/etc/hosts.allow\"
.if !defined(NOINET6)
CFLAGS+=-DINET6
.endif

DPADD=	${LIBWRAP}
LDADD=	-lwrap

.PATH:	${.CURDIR}/../../contrib/tcp_wrappers

.include <bsd.prog.mk>
