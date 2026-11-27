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

CXXFLAGS+=	-I${SRCTOP}/contrib/lutok/include \
		-I${SRCTOP}/lib/liblua	\
		-I${SRCTOP}/contrib/lua/src

WARNS?=	3
SHLIB_MAJOR=	4

CXXSTD=		gnu++20

.include <bsd.lib.mk>
