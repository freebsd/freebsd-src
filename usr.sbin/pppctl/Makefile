# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.16.26.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	pppctl
MAN=	pppctl.8

DPADD=	${LIBPTHREAD} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lpthread -ledit -ltermcap

.include <bsd.prog.mk>
