# $FreeBSD$

LIBUCL=	${SRCTOP}/contrib/libucl

PACKAGE=lib${LIB}
LIB=		ucl
PRIVATELIB=	true
SHLIB_MAJOR=	1
SRCS=		ucl_emitter_streamline.c \
		ucl_emitter_utils.c \
		ucl_emitter.c \
		ucl_hash.c \
		ucl_msgpack.c \
		ucl_parser.c  \
		ucl_schema.c \
		ucl_sexp.c \
		ucl_util.c

.PATH:		${LIBUCL}/src \
		${LIBUCL}/include

INCS=		ucl.h
LIBADD=		m

WARNS=		1
CFLAGS+=	-I${LIBUCL}/include \
		-I${LIBUCL}/src \
		-I${LIBUCL}/uthash \
		-I${LIBUCL}/klib

# FIXME: for some reason clang crashes at -O0
# We can't change CHERI_OPTIMIZATION_FLAGS as the value set on the command line
# overrides any value set here. We need to use _CHERI_CFLAGS instead as it gets
# added right at the end of the compilation command and therefore overrides
# whatever CHERI_OPTIMIZATION_FLAGS sets
_CHERI_CFLAGS+=-O2

.include <bsd.lib.mk>
