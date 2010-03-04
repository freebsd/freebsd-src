# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/netcat

PROG=	nc
SRCS=	netcat.c atomicio.c socks.c

CFLAGS+=-DIPSEC
LDADD=	-lipsec
DPADD=	${LIBIPSEC}

WARNS?=	1

.include <bsd.prog.mk>
