# $FreeBSD: src/usr.sbin/pkg_install/create/Makefile,v 1.10.2.1 1999/08/29 15:45:13 peter Exp $

PROG= pkg_create

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFTPIO} ${LIBMD}
LDADD=	${LIBINSTALL} -lftpio -lmd

SRCS= main.c perform.c pl.c

.include <bsd.prog.mk>
