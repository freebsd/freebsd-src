# $FreeBSD$

PROG=	pkg_version
SRCS=	main.c perform.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	2

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

test:
	./test-pkg_version.sh

.include <bsd.prog.mk>
