# $FreeBSD: src/usr.sbin/pkg_install/delete/Makefile,v 1.11.2.3 2002/08/31 19:31:44 obrien Exp $

PROG=	pkg_delete
SRCS=	main.c perform.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	2

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
