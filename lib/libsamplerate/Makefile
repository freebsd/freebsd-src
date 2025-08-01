.PATH:		${SRCTOP}/contrib/libsamplerate

LIB=		samplerate
INTERNALLIB=

SRCS=		samplerate.c \
		src_linear.c \
		src_sinc.c \
		src_zoh.c

INCS=		samplerate.h

CFLAGS+=	-DENABLE_SINC_BEST_CONVERTER \
		-DENABLE_SINC_MEDIUM_CONVERTER \
		-DENABLE_SINC_FAST_CONVERTER \
		-I${SRCTOP}/contrib/libsamplerate

.include <bsd.lib.mk>
