#
# $FreeBSD: src/usr.sbin/tcpdchk/Makefile,v 1.1.2.2 1999/08/29 15:48:33 peter Exp $
#

MAINTAINER=markm@FreeBSD.org

PROG=	tcpdchk
MAN8=	tcpdchk.8
SRCS=	tcpdchk.c fakelog.c inetcf.c scaffold.c

CFLAGS=	-DREAL_DAEMON_DIR=\"/usr/libexec\" \
	-DSEVERITY=LOG_INFO -DRFC931_TIMEOUT=10 \
	-DHOSTS_DENY=\"/etc/hosts.deny\" -DHOSTS_ALLOW=\"/etc/hosts.allow\"

DPADD=	${LIBWRAP}
LDADD=	-lwrap

.PATH:	${.CURDIR}/../../contrib/tcp_wrappers

.include <bsd.prog.mk>
