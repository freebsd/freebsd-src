# $FreeBSD$

LIB=	install
INTERNALLIB= YES
SRCS=	file.c msg.c plist.c str.c exec.c global.c pen.c match.c \
	deps.c version.c pkgwrap.c url.c

CFLAGS+= ${DEBUG}

WARNS?=	3
WFORMAT?=	1

.include <bsd.lib.mk>
