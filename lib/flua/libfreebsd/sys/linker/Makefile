SHLIB_NAME=	linker.so
SHLIBDIR=	${LIBDIR}/flua/freebsd/sys

SRCS+=		lua_freebsd_sys_linker.c

CFLAGS+= \
	-I${SRCTOP}/contrib/lua/src \
	-I${SRCTOP}/lib/liblua \

MAN=	freebsd.sys.linker.3lua

.include <bsd.lib.mk>
