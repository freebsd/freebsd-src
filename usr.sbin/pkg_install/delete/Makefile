# $FreeBSD: src/usr.sbin/pkg_install/delete/Makefile,v 1.10.2.1 1999/08/29 15:45:17 peter Exp $

PROG= pkg_delete
CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFTPIO} ${LIBMD}
LDADD=	${LIBINSTALL} -lftpio -lmd

SRCS= main.c perform.c

.include <bsd.prog.mk>
