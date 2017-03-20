#	$NetBSD: Makefile,v 1.2 2014/04/29 01:21:02 christos Exp $
#	$FreeBSD$
.include <bsd.own.mk>

PROG=	getaddrinfo

CFLAGS+=	-I${.CURDIR}/../../lib/libnetbsd
LIBNETBSDDIR=	${.OBJDIR}/../../lib/libnetbsd
LIBNETBSD=	${LIBNETBSDDIR}/libnetbsd.a
DPADD+=		${LIBNETBSD}
LDADD+=		${LIBNETBSD}

LIBADD+=	util

SYS_SOCKET_H?=	${.CURDIR}/../../sys/sys/socket.h

CFLAGS+=	-I.
DPSRCS+=	tables.h
CLEANFILES+=	tables.h
tables.h: tables.awk ${SYS_SOCKET_H}
	LC_ALL=C awk -f ${.ALLSRC} > ${.TARGET}
.include <bsd.prog.mk>
