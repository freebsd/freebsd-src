# $FreeBSD$

LIBXODIR=       ${STAGEDIR}${PREFIX}/usr/lib/libxo/encoder
SHLIBDIR?=      ${LIBXODIR}
LIBDIR?=        ${LIBXODIR}

.include <src.opts.mk>

PACKAGE=	runtime

LIBXOSRC=	${SRCTOP}/contrib/libxo

.PATH:	${LIBXOSRC}/encoder/csv

LIB=	enc_csv
SHLIB_MAJOR=0

SRCS=	enc_csv.c

CFLAGS+=-I${LIBXOSRC}/libxo -I${.CURDIR}
CFLAGS+=-DXO_ENCODERDIR=\"/usr/lib/libxo/encoder\"

LIBADD=	util xo
LDFLAGS += -L${.OBJDIR}/../../wildebeast

WARNS?= 5

SYMLINKS+=      ${SHLIB_NAME} ${LIBXODIR}/csv.enc

.include <bsd.lib.mk>
