# $FreeBSD$

PROG=	pppctl
MAN=	pppctl.8
CFLAGS+=-Wall
LDADD+= -lc_r -ledit -ltermcap
DPADD+= ${LIBC_R} ${LIBEDIT} ${LIBTERMCAP}

.include <bsd.prog.mk>
