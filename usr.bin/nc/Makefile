# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/netcat

PROG=	nc
SRCS=	netcat.c atomicio.c socks.c

CFLAGS+=-DIPSEC
LDADD+=	-lipsec

.include <bsd.prog.mk>
