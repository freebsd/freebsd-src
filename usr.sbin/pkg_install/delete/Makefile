# $FreeBSD$

PROG= pkg_delete

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	2

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

SRCS= main.c perform.c

.include <bsd.prog.mk>
