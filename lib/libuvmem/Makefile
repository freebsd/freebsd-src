PACKAGE=	runtime

LIB=		uvmem
SRCS=		subr_vmem.c

SHLIB_MAJOR=	1
LIBADD+=	pthread
CFLAGS.clang+=-Wno-thread-safety-analysis

SYMBOL_MAPS=    ${.CURDIR}/Symbol.map
VERSION_DEF=	${SRCTOP}/lib/libc/Versions.def

MAN=		libuvmem.3

.include <src.opts.mk>

.PATH:	${SRCTOP}/sys/kern

.include <bsd.lib.mk>
