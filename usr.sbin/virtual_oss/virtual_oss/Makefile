PROG=		virtual_oss
MAN=		${PROG}.8

SRCS=		audio_delay.c \
		compressor.c \
		ctl.c \
		eq.c \
		format.c \
		httpd.c \
		main.c \
		mul.c \
		ring.c \
		virtual_oss.c

CFLAGS+=	-I${SRCTOP}/contrib/libsamplerate
# The --export-dynamic-symbol flags below are needed because some backends make
# use of those symbols.
LDFLAGS+= 	-lpthread -lcuse -lnv -lm \
		-Wl,--export-dynamic-symbol=virtual_oss_wait \
		-Wl,--export-dynamic-symbol=voss_has_synchronization
LIBADD=		samplerate
LDFLAGS+=	-L${.OBJDIR:H:H}/libsamplerate

.include <bsd.prog.mk>
