# $FreeBSD: src/usr.sbin/pkg_install/create/Makefile,v 1.11 1999/08/28 01:17:57 peter Exp $

PROG= pkg_create

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFTPIO} ${LIBMD}
LDADD=	${LIBINSTALL} -lftpio -lmd

SRCS= main.c perform.c pl.c

.include <bsd.prog.mk>
