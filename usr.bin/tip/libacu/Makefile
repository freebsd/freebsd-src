# $FreeBSD$

LIB=		acu
INTERNALLIB=	YES
CFLAGS+=	-I${.CURDIR}/../tip
SRCS=		acucommon.c biz22.c courier.c df.c dn11.c hayes.c \
		multitech.c t3000.c tod.c unidialer.c v3451.c v831.c ventel.c

$(OBJS):	${.CURDIR}/../tip/tipconf.h

.include <bsd.lib.mk>
