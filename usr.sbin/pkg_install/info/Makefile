# $FreeBSD: src/usr.sbin/pkg_install/info/Makefile,v 1.11.2.3 2002/08/31 19:33:55 obrien Exp $

PROG=	pkg_info
SRCS=	main.c perform.c show.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	2

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
