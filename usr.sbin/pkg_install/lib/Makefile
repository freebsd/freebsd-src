# $FreeBSD$

LIB=	install
SRCS=	file.c msg.c plist.c str.c exec.c global.c pen.c match.c \
	deps.c version.c
NOPROFILE= yes
NOPIC=	yes

CFLAGS+= ${DEBUG}

WARNS?=	2

install:
	@echo -n

.include <bsd.lib.mk>
