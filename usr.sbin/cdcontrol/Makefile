# $FreeBSD$

PROG= cdcontrol
CFLAGS+= -Wall

DPADD += ${LIBEDIT} ${LIBTERMCAP}
LDADD += -ledit -ltermcap

.include <bsd.prog.mk>
