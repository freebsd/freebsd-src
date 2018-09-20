# $FreeBSD$

.include <src.opts.mk>

CRTARCH=	${MACHINE_CPUARCH:C/amd64/x86_64/}
CRTSRC=		${SRCTOP}/contrib/compiler-rt

.PATH:		${CRTSRC}/lib

CLANGDIR=	/usr/lib/clang/6.0.1
LIBDIR=		${CLANGDIR}/lib/freebsd
SHLIBDIR=	${LIBDIR}

NO_PIC=
MK_PROFILE=	no

WARNS?=		0

SSP_CFLAGS=
CFLAGS+=	-DNDEBUG
CFLAGS+=	-DHAVE_RPC_XDR_H=0
CFLAGS+=	-DHAVE_TIRPC_RPC_XDR_H=0
CFLAGS+=	-DSANITIZER_SUPPORTS_WEAK_HOOKS=0
CFLAGS+=	-DUBSAN_CAN_USE_CXXABI
CFLAGS+=	${PICFLAG}
CFLAGS+=	-fno-builtin
CFLAGS+=	-fno-exceptions
CFLAGS+=	-fno-rtti
.if ${COMPILER_VERSION} >= 30700
CFLAGS+=	-fno-sanitize=safe-stack
.endif
CFLAGS+=	-fno-stack-protector
CFLAGS+=	-funwind-tables
CFLAGS+=	-fvisibility-inlines-hidden
CFLAGS+=	-fvisibility=hidden
CFLAGS+=	-I${CRTSRC}/lib
CXXFLAGS+=	-std=c++11
