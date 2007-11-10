# $FreeBSD$

PROG=	pkg_updating
SRCS=	main.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?= 1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
