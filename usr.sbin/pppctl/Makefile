# $FreeBSD$

PROG=	pppctl
SRCS=	pppctl.c
CFLAGS+=-Wall
LDADD+= -ledit -ltermcap
DPADD+= ${LIBEDIT} ${LIBTERMCAP}
MAN8=	pppctl.8

.include <bsd.prog.mk>
