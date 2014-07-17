# $FreeBSD$

PROG=	pppctl
MAN=	pppctl.8

WARNS?=	2

DPADD=	${LIBPTHREAD} ${LIBEDIT} ${LIBTERMCAPW}
LDADD=	-lpthread -ledit -ltermcapw

.include <bsd.prog.mk>
