# $FreeBSD: src/usr.bin/tip/libacu/Makefile,v 1.3.12.1 2002/07/19 18:46:29 ru Exp $

LIB=		acu
INTERNALLIB=	YES
CFLAGS+=	-I${.CURDIR}/../tip
SRCS=		acucommon.c biz22.c courier.c df.c dn11.c hayes.c \
		multitech.c t3000.c tod.c unidialer.c v3451.c v831.c ventel.c

$(OBJS):	${.CURDIR}/../tip/tipconf.h

.include <bsd.lib.mk>
