# $FreeBSD: src/usr.sbin/pkg_install/info/Makefile,v 1.11 1999/08/28 01:18:02 peter Exp $

PROG= pkg_info
CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBFTPIO} ${LIBMD}
LDADD=	${LIBINSTALL} -lftpio -lmd

SRCS= main.c perform.c show.c

.include <bsd.prog.mk>
