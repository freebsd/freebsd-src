#$FreeBSD$

LIB=	heimipcs
PRIVATELIB=
LIBADD=	heimbase roken pthread

SRCS=	\
	server.c \
	common.c

CFLAGS+=	-I${KRB5DIR}/lib/roken -I${.OBJDIR:H}/libroken \
		-I${KRB5DIR}/lib/asn1 -I${.OBJDIR:H}/libasn1 \
		-I${KRB5DIR}/base \
		-I${KRB5DIR}/lib/ipc -I.

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/ipc
