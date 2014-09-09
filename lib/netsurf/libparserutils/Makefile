#
# $FreeBSD$
#

.include <bsd.own.mk>

NETSURF_DISTDIR?= ${.CURDIR}/../../../contrib/netsurf

LIB=	parserutils

.PATH:		${NETSURF_DISTDIR}/libparserutils
SRCS=		src/charset/aliases.c \
		src/charset/codec.c \
		src/charset/codecs/codec_ascii.c \
		src/charset/codecs/codec_8859.c \
		src/charset/codecs/codec_ext8.c \
		src/charset/codecs/codec_utf8.c \
		src/charset/codecs/codec_utf16.c \
		src/charset/encodings/utf8.c \
		src/charset/encodings/utf16.c \
		src/input/filter.c \
		src/input/inputstream.c \
		src/utils/buffer.c \
		src/utils/errors.c \
		src/utils/stack.c \
		src/utils/vector.c

WARNFLAGS=	-Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings \
		-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations \
		-Wnested-externs -pedantic -Werror

CFLAGS+=	-I${NETSURF_DISTDIR}/libparserutils/include \
		-I${NETSURF_DISTDIR}/libparserutils/src \
		-I${.CURDIR}/src/charset \
		-D_LONGLONG -D_BSD_SOURCE \
		'-D_ALIGNED=__attribute__((aligned))' -DNDEBUG $(WARNFLAGS)

.include <bsd.lib.mk>
.include <bsd.subdir.mk>
