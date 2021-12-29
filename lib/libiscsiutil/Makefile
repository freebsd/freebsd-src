LIB=		iscsiutil
INTERNALLIB=
PACKAGE=	iscsi

INCS=		libiscsiutil.h

SRCS=		chap.c connection.c keys.c log.c pdu.c text.c utils.c
CFLAGS+=	-I${SRCTOP}/sys/dev/iscsi

.include <bsd.lib.mk>
