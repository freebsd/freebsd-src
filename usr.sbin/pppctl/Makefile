# $FreeBSD$

PROG=	pppctl
SRCS=	pppctl.c
CFLAGS+=-Wall
LDADD+= -pthread -ledit -ltermcap
DPADD+= ${LIBC_R} ${LIBEDIT} ${LIBTERMCAP}
MAN8=	pppctl.8

.include <bsd.prog.mk>
