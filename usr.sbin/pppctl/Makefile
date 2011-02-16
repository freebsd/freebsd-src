# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.16.30.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	pppctl
MAN=	pppctl.8

DPADD=	${LIBPTHREAD} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lpthread -ledit -ltermcap

.include <bsd.prog.mk>
