# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.4.2.1 1999/08/29 15:46:51 peter Exp $

PROG=	pppctl
SRCS=	pppctl.c
CFLAGS+=-Wall
LDADD+= -ledit -ltermcap
DPADD+= ${LIBEDIT} ${LIBTERMCAP}
MAN8=	pppctl.8

.include <bsd.prog.mk>
