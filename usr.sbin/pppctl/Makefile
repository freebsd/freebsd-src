# $FreeBSD$

PROG=	pppctl
MAN=	pppctl.8
LDADD+= -lc_r -ledit -ltermcap
DPADD+= ${LIBC_R} ${LIBEDIT} ${LIBTERMCAP}

.include <bsd.prog.mk>
