# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.5 1999/08/28 01:18:57 peter Exp $

PROG=	pppctl
SRCS=	pppctl.c
CFLAGS+=-Wall
LDADD+= -ledit -ltermcap
DPADD+= ${LIBEDIT} ${LIBTERMCAP}
MAN8=	pppctl.8

.include <bsd.prog.mk>
