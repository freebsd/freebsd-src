LIB=	cron

SRCS=	entry.c env.c misc.c
CFLAGS+=-I${.CURDIR}/../cron
CFLAGS+=-DLOGIN_CAP
NOPIC=		yes
NOPROFILE=	yes

install:
	@echo -n

.include <bsd.lib.mk>
