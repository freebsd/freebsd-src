# $FreeBSD$

PROG= pkg_add

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

WARNS?=	2

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

SRCS= main.c perform.c futil.c extract.c

.include <bsd.prog.mk>
