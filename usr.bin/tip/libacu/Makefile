LIB=		acu
CFLAGS+=	-I${.CURDIR}/../tip
SRCS=		acucommon.c biz22.c courier.c df.c dn11.c hayes.c \
		multitech.c t3000.c tod.c unidialer.c v3451.c v831.c ventel.c
NOPROFILE=	yes

$(OBJS):	${.CURDIR}/../tip/tipconf.h

install:
	@echo -n

.include <bsd.lib.mk>
