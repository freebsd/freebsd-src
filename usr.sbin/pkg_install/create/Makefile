# $FreeBSD: src/usr.sbin/pkg_install/create/Makefile,v 1.11.2.1 2000/07/16 01:58:42 des Exp $

PROG= pkg_create

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

SRCS= main.c perform.c pl.c

.include <bsd.prog.mk>
