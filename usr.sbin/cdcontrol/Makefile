# $FreeBSD$

PROG= cdcontrol
MAN1=	${PROG}.1
CFLAGS+= -Wall

DPADD += ${LIBEDIT} ${LIBTERMCAP}
LDADD += -ledit -ltermcap

.include <bsd.prog.mk>
