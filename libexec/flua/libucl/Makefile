SHLIB_NAME=	ucl.so

WARNS=		2

UCLSRC?= 	${SRCTOP}/contrib/libucl
.PATH: ${UCLSRC}/lua
SRCS+=		lua_ucl.c
CFLAGS+=	\
		-I${UCLSRC}/include \
		-I${UCLSRC}/src \
		-I${UCLSRC}/uthash
LIBADD+=	ucl

.include <bsd.lib.mk>
