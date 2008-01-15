# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.16 2004/01/31 17:28:09 marcel Exp $

PROG=	pppctl
MAN=	pppctl.8

DPADD=	${LIBPTHREAD} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lpthread -ledit -ltermcap

.include <bsd.prog.mk>
