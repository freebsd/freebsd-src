# $FreeBSD: src/usr.sbin/pkg_install/delete/Makefile,v 1.11.2.1 2000/07/16 01:58:42 des Exp $

PROG= pkg_delete
CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

SRCS= main.c perform.c

.include <bsd.prog.mk>
