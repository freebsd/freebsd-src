# $FreeBSD$

PROG=	pppctl
MAN=	pppctl.8

DPADD=	${LIBC_R} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	-lpthread -ledit -ltermcap

.include <bsd.prog.mk>
