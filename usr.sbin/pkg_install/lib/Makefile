# $FreeBSD: src/usr.sbin/pkg_install/lib/Makefile,v 1.6.8.5 2002/07/19 18:46:30 ru Exp $

LIB=	install
INTERNALLIB= YES
SRCS=	file.c msg.c plist.c str.c exec.c global.c pen.c match.c \
	deps.c version.c

CFLAGS+= ${DEBUG}

WARNS?=	2

.include <bsd.lib.mk>
