# $FreeBSD$

PROG=	pppctl
MAN=	pppctl.8
CFLAGS+=-Wall
LDADD+= -ledit -ltermcap
DPADD+= ${LIBEDIT} ${LIBTERMCAP}

.include <bsd.prog.mk>
