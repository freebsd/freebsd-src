# $FreeBSD: src/usr.sbin/cdcontrol/Makefile,v 1.2.2.1 2001/04/25 12:09:22 ru Exp $

PROG= cdcontrol
CFLAGS+= -Wall

DPADD += ${LIBEDIT} ${LIBTERMCAP}
LDADD += -ledit -ltermcap

.include <bsd.prog.mk>
