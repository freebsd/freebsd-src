# $FreeBSD: src/usr.sbin/pkg_install/lib/Makefile,v 1.6.8.7 2004/08/11 16:16:23 eik Exp $

LIB=	install
INTERNALLIB= YES
SRCS=	file.c msg.c plist.c str.c exec.c global.c pen.c match.c \
	deps.c version.c pkgwrap.c url.c

CFLAGS+= ${DEBUG}

WARNS?=	3
WFORMAT?=	1

.include <bsd.lib.mk>
