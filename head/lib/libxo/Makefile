# $FreeBSD$

LIBXO=	${.CURDIR:H:H}/contrib/libxo

.PATH:	${LIBXO}/libxo

LIB=	xo
SHLIB_MAJOR=0

SRCS=	libxo.c

CFLAGS+=-I${LIBXO}/libxo

INCS=	xo.h
INCSDIR=${INCLUDEDIR}/libxo

MAN+=	libxo.3
MAN+=	xo_attr.3 \
	xo_create.3 \
	xo_emit.3 \
	xo_err.3 \
	xo_finish.3 \
	xo_flush.3 \
	xo_no_setlocale.3 \
	xo_open_container.3 \
	xo_open_list.3 \
	xo_parse_args.3 \
	xo_set_allocator.3 \
	xo_set_flags.3 \
	xo_set_info.3 \
	xo_set_options.3 \
	xo_set_style.3 \
	xo_set_writer.3
MAN+=	xo_format.5

.include <bsd.lib.mk>
