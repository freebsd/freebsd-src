# $FreeBSD: src/usr.sbin/pkg_install/lib/Makefile,v 1.6.8.2 2001/03/20 09:45:55 sobomax Exp $

LIB=	install
SRCS=	file.c msg.c plist.c str.c exec.c global.c pen.c match.c \
	deps.c
CFLAGS+= ${DEBUG}
NOPROFILE= yes
NOPIC=	yes

install:
	@echo -n

.include <bsd.lib.mk>
