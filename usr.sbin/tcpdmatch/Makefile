#
# $Id: Makefile,v 1.1.2.1 1999/04/28 10:02:07 obrien Exp $
#

MAINTAINER=markm@FreeBSD.org

PROG=	tcpdmatch
MAN8=	tcpdmatch.8
SRCS=	tcpdmatch.c fakelog.c inetcf.c scaffold.c

CFLAGS=	-DREAL_DAEMON_DIR=\"/usr/libexec\" \
	-DSEVERITY=LOG_INFO -DRFC931_TIMEOUT=10

DPADD=	${LIBWRAP}
LDADD=	-lwrap

.PATH:	${.CURDIR}/../../contrib/tcp_wrappers

.include <bsd.prog.mk>
