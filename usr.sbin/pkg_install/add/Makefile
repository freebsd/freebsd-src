# $FreeBSD: src/usr.sbin/pkg_install/add/Makefile,v 1.11.2.3 2002/08/31 18:41:10 obrien Exp $

PROG=	pkg_add
SRCS=	main.c perform.c futil.c extract.c

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	2

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

.include <bsd.prog.mk>
