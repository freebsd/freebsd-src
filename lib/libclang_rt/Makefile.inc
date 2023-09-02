
.include <bsd.compiler.mk>

PACKAGE=	clang
CRTSRC=		${SRCTOP}/contrib/llvm-project/compiler-rt
.include "compiler-rt-vars.mk"

.PATH:		${CRTSRC}/lib

LIBDIR=		${SANITIZER_LIBDIR}
SHLIBDIR=	${LIBDIR}

NO_PIC=
MK_PROFILE=	no
MK_ASAN:=	no
MK_SSP=		no
MK_UBSAN:=	no

WARNS?=		0

CFLAGS+=	-DNDEBUG
CFLAGS+=	-DHAVE_RPC_XDR_H=0
CFLAGS+=	-DSANITIZER_SUPPORTS_WEAK_HOOKS=0
CFLAGS+=	-DUBSAN_CAN_USE_CXXABI
CFLAGS+=	${PICFLAG}
CFLAGS+=	-fno-builtin
CFLAGS+=	-fno-exceptions
CXXFLAGS+=	-fno-rtti
.if ${COMPILER_TYPE} == clang
CFLAGS+=	-fno-sanitize=safe-stack
.endif
CFLAGS+=	-fno-stack-protector
CFLAGS+=	-funwind-tables
CXXFLAGS+=	-fvisibility-inlines-hidden
CXXFLAGS+=	-fvisibility=hidden
CFLAGS+=	-I${CRTSRC}/include
CFLAGS+=	-I${CRTSRC}/lib
CXXSTD=		c++17
