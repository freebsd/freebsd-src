# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.16.28.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	pppctl
MAN=	pppctl.8

DPADD=	${LIBPTHREAD} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lpthread -ledit -ltermcap

.include <bsd.prog.mk>
