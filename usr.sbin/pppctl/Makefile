# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.16.32.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	pppctl
MAN=	pppctl.8

DPADD=	${LIBPTHREAD} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lpthread -ledit -ltermcap

.include <bsd.prog.mk>
