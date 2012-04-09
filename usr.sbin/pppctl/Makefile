# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.16.30.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	pppctl
MAN=	pppctl.8

DPADD=	${LIBPTHREAD} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lpthread -ledit -ltermcap

.include <bsd.prog.mk>
