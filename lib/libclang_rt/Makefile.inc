# $FreeBSD$

.include <src.opts.mk>

# NOTE: based on TARGET_ABI/TARGET_CPUTYPE, set in Makefile.inc1 .
.if ${MACHINE} == "arm"
.if ${MACHINE_ARCH:Marmv[67]*} != "" && ${CPUTYPE:M*soft*} == ""
CRTARCH=	armhf
.endif
.endif

CRTARCH?=	${TARGET_CPUARCH:C/amd64/x86_64/}
CRTSRC=		${SRCTOP}/contrib/compiler-rt

.PATH:		${CRTSRC}/lib

CLANGDIR=	/usr/lib/clang/5.0.0
LIBDIR=		${CLANGDIR}/lib/freebsd

NO_PIC=
MK_PROFILE=	no

WARNS?=		0

SSP_CFLAGS=
CFLAGS+=	-DNDEBUG
CFLAGS+=	${PICFLAG}
CFLAGS+=	-fno-builtin
CFLAGS+=	-fno-exceptions
CXXFLAGS+=	-fno-rtti
.if ${COMPILER_TYPE} == clang && ${COMPILER_VERSION} >= 30700
CFLAGS+=	-fno-sanitize=safe-stack
.endif
CFLAGS+=	-fno-stack-protector
CFLAGS+=	-funwind-tables
CXXFLAGS+=	-fvisibility-inlines-hidden
CXXFLAGS+=	-fvisibility=hidden
CFLAGS+=	-I${CRTSRC}/lib
CXXFLAGS+=	-std=c++11
