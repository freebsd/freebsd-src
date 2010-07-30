# $FreeBSD$

PROG=	pkg_create
SRCS=	main.c perform.c pl.c

WARNS?=	3
WFORMAT?=	1

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
