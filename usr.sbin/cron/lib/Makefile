# $FreeBSD$

LIB=	cron
NOPIC=		yes
NOPROFILE=	yes
SRCS= entry.c env.c misc.c

CFLAGS+= -I${.CURDIR}/../cron
CFLAGS+= -DLOGIN_CAP

install:
	@echo -n

.include <bsd.lib.mk>
