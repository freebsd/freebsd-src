# $FreeBSD: src/usr.sbin/pkg_install/add/Makefile,v 1.11.2.1 2000/07/16 01:58:41 des Exp $

PROG= pkg_add

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

SRCS= main.c perform.c futil.c extract.c

.include <bsd.prog.mk>
