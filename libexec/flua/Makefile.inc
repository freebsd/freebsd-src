PACKAGE=	flua

SHLIBDIR?=	${LIBDIR}/flua

CFLAGS+=	\
		-I${SRCTOP}/contrib/lua/src \
		-I${SRCTOP}/lib/liblua
