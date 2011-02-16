# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.16.34.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	pppctl
MAN=	pppctl.8

DPADD=	${LIBPTHREAD} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lpthread -ledit -ltermcap

.include <bsd.prog.mk>
