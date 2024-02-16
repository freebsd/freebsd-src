SHLIB_NAME=	hash.so
SHLIBDIR=	${LIBDIR}/flua

SRCS+=		lhash.c

CFLAGS+= \
	-I${SRCTOP}/contrib/lua/src \
	-I${SRCTOP}/lib/liblua \

LIBADD+=	md

MAN=	hash.3lua

.include <bsd.lib.mk>
