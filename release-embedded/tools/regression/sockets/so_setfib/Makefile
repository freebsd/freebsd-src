# $FreeBSD$

PROG=	so_setfib
NO_MAN=
WARNS?=	6

.ifdef INET6
CFLAGS+=	-DINET6
.endif
.ifdef INET
CFLAGS+=	-DINET
.endif

.include <bsd.prog.mk>
