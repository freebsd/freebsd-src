# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.16.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	pppctl
MAN=	pppctl.8

DPADD=	${LIBPTHREAD} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lpthread -ledit -ltermcap

.include <bsd.prog.mk>
