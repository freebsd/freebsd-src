# $FreeBSD: src/usr.sbin/pppctl/Makefile,v 1.5.2.1 2001/04/25 12:10:34 ru Exp $

PROG=	pppctl
MAN=	pppctl.8
CFLAGS+=-Wall
LDADD+= -ledit -ltermcap
DPADD+= ${LIBEDIT} ${LIBTERMCAP}

.include <bsd.prog.mk>
