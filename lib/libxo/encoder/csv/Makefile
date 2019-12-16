# $FreeBSD$

.include <src.opts.mk>

PACKAGE=	runtime

LIBXOSRC=	${SRCTOP}/contrib/libxo

.PATH:	${LIBXOSRC}/encoder/csv

# We use FILES to install the encoder library under the proper/magic
# name, in the proper/magic directory.  libxo looks for encoders from
# "--libxo encoder=name" as "${prefix}/lib/libxo/encoder/${name}.enc"
FILES = libenc_csv.so
FILESNAME_libenc_csv.so= csv.enc
FILESDIR = /usr/lib/libxo/encoder

LIB=	enc_csv
SHLIB_MAJOR=0
SHLIB_NAME=     lib${LIB}.so

SRCS=	enc_csv.c

CFLAGS+=-I${LIBXOSRC}/libxo -I${.CURDIR}
CFLAGS+=-DXO_ENCODERDIR=\"/usr/lib/libxo/encoder\"

LIBADD=	util xo
LDFLAGS += -L${.OBJDIR:H:H}/libxo

WARNS?= 5

# Need to define a fake "install" target to block the one in bsd.lib.mk,
# since that one will install our lib in the wrong place (/usr/lib/).
install: realinstall

.include <bsd.lib.mk>
.include <bsd.files.mk>
