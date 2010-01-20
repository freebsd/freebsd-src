# $FreeBSD$

PROG=	pkg_version
SRCS=	main.c perform.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

test:
	sh ${.CURDIR}/test-pkg_version.sh

.include <bsd.prog.mk>
