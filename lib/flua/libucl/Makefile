SHLIB_NAME=	ucl.so
SHLIBDIR=	${LIBDIR}/flua

WARNS=		2

UCLSRC?= 	${SRCTOP}/contrib/libucl
.PATH: ${UCLSRC}/lua
SRCS+=		lua_ucl.c
CFLAGS+=	\
		-I${SRCTOP}/contrib/lua/src \
		-I${SRCTOP}/lib/liblua \
		-I${UCLSRC}/include \
		-I${UCLSRC}/src \
		-I${UCLSRC}/uthash
LIBADD+=	ucl

.include <bsd.lib.mk>
