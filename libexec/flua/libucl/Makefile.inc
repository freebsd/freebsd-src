.if ${WARNS:U6} > 2
WARNS=		2
.endif

UCLSRC?=	${SRCTOP}/contrib/libucl
.PATH: ${UCLSRC}/lua
SRCS+=		lua_ucl.c
CFLAGS+=	\
		-I${UCLSRC}/include \
		-I${UCLSRC}/src \
		-I${UCLSRC}/uthash
LIBADD+=	ucl
