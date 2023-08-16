
LIBUCL_DIR=	${SRCTOP}/contrib/libucl

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

.PATH:		${LIBUCL_DIR}/src \
		${LIBUCL_DIR}/include \
		${LIBUCL_DIR}/doc

INCS=		ucl.h
LIBADD=		m

WARNS?=		1
CFLAGS+=	-I${LIBUCL_DIR}/include \
		-I${LIBUCL_DIR}/src \
		-I${LIBUCL_DIR}/uthash \
		-I${LIBUCL_DIR}/klib

MAN+=		libucl.3

.include <bsd.lib.mk>
