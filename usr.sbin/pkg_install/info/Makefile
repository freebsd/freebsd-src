# $FreeBSD: src/usr.sbin/pkg_install/info/Makefile,v 1.11.2.2 2001/10/23 09:16:03 sobomax Exp $

PROG= pkg_info

CFLAGS+= ${DEBUG} -I${.CURDIR}/../lib

WARNS?=	2

DPADD=	${LIBINSTALL} ${LIBFETCH} ${LIBMD}
LDADD=	${LIBINSTALL} -lfetch -lmd

SRCS= main.c perform.c show.c

.include <bsd.prog.mk>
