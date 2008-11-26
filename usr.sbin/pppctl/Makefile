# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.16.24.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	pppctl
MAN=	pppctl.8

DPADD=	${LIBPTHREAD} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lpthread -ledit -ltermcap

.include <bsd.prog.mk>
