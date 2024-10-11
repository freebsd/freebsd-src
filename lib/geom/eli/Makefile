PACKAGE=geom
.PATH: ${SRCTOP}/sys/geom/eli

GEOM_CLASS=	eli
SRCS=	g_eli_crypto.c
SRCS+=	g_eli_hmac.c
SRCS+=	g_eli_key.c
SRCS+=	pkcs5v2.c

LIBADD=	md crypto

CFLAGS+=-I${SRCTOP}/sys

.include <bsd.lib.mk>
