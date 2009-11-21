# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.16.30.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	pppctl
MAN=	pppctl.8

DPADD=	${LIBPTHREAD} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lpthread -ledit -ltermcap

.include <bsd.prog.mk>
