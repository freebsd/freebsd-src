#$FreeBSD$

LIB=	heimipcc
PRIVATELIB=
LIBADD=	heimbase roken pthread

SRCS=	\
	client.c \
	common.c

CFLAGS+=	-I${KRB5DIR}/lib/roken \
		-I${.OBJDIR:H}/libroken \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/base \
		-I${KRB5DIR}/lib/ipc \
		-I${KRB5DIR}/include

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/ipc
