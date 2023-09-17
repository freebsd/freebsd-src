
PACKAGE=ssh
LIB=	cbor
PRIVATELIB=

DIST=${SRCTOP}/contrib/libcbor
.PATH:	${DIST}/src

SRCS=	allocators.c
SRCS+=	cbor.c
SRCS+=	cbor/arrays.c
SRCS+=	cbor/bytestrings.c
SRCS+=	cbor/callbacks.c
SRCS+=	cbor/common.c
SRCS+=	cbor/encoding.c
SRCS+=	cbor/floats_ctrls.c
SRCS+=	cbor/ints.c
SRCS+=	cbor/maps.c
SRCS+=	cbor/serialization.c
SRCS+=	cbor/streaming.c
SRCS+=	cbor/strings.c
SRCS+=	cbor/tags.c
SRCS+=	cbor/internal/builder_callbacks.c
SRCS+=	cbor/internal/encoders.c
SRCS+=	cbor/internal/loaders.c
SRCS+=	cbor/internal/memory_utils.c
SRCS+=	cbor/internal/stack.c
SRCS+=	cbor/internal/unicode.c

CFLAGS+= -I${DIST}/src -I${.CURDIR}

WARNS?=	2
MAN=

.include <bsd.lib.mk>
