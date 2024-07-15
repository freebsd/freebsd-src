.include <src.opts.mk>

PACKAGE=	runtime

LIBXOSRC=	${SRCTOP}/contrib/libxo

.PATH:	${LIBXOSRC}/encoder/csv

SHLIBDIR=	${LIBDIR}/libxo/encoder
SHLIB_NAME=	csv.enc

SRCS=	enc_csv.c

CFLAGS+=-I${LIBXOSRC}/libxo -I${.CURDIR}
CFLAGS+=-DXO_ENCODERDIR=\"${LIBDIR}/libxo/encoder\"

LIBADD=	util xo
LDFLAGS += -L${.OBJDIR:H:H}/libxo

WARNS?= 5

.include <bsd.lib.mk>
