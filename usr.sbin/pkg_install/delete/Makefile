# $FreeBSD$

PROG=	pkg_delete
SRCS=	main.c perform.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
