LIB=		iscsiutil
INTERNALLIB=

INCS=		libiscsiutil.h
SRCS=		chap.c connection.c keys.c log.c pdu.c text.c utils.c
CFLAGS+=	-I${SRCTOP}/sys/dev/iscsi
LIBADD=		md

.include <bsd.lib.mk>
