# $FreeBSD$

PROG=	pppctl
CFLAGS+=-Wall
LDADD+= -lc_r -ledit -ltermcap
DPADD+= ${LIBC_R} ${LIBEDIT} ${LIBTERMCAP}

.include <bsd.prog.mk>
