# $FreeBSD: src/usr.sbin/pkg_install/lib/Makefile,v 1.14 2002/11/06 08:57:03 obrien Exp $

LIB=	install
INTERNALLIB= YES
SRCS=	file.c msg.c plist.c str.c exec.c global.c pen.c match.c \
	deps.c version.c pkgwrap.c url.c

CFLAGS+= ${DEBUG}

WARNS?=	2

.include <bsd.lib.mk>
