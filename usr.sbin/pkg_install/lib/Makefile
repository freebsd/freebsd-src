# $FreeBSD: src/usr.sbin/pkg_install/lib/Makefile,v 1.6.8.6 2003/08/11 01:13:44 kris Exp $

LIB=	install
INTERNALLIB= YES
SRCS=	file.c msg.c plist.c str.c exec.c global.c pen.c match.c \
	deps.c version.c pkgwrap.c

CFLAGS+= ${DEBUG}

WARNS?=	2

.include <bsd.lib.mk>
