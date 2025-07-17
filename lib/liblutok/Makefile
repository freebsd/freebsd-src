LIB_CXX=	lutok
INTERNALLIB=

LIBADD=		lua

.PATH: ${SRCTOP}/contrib/lutok
SRCS=	c_gate.cpp		\
	debug.cpp		\
	exceptions.cpp		\
	operations.cpp		\
	stack_cleaner.cpp	\
	state.cpp

CFLAGS+=	-I${SRCTOP}/contrib/lutok/include \
		-I${SRCTOP}/lib/liblua	\
		-I${SRCTOP}/contrib/lua/src

WARNS?=	3
SHLIB_MAJOR=	0

.include <bsd.lib.mk>
