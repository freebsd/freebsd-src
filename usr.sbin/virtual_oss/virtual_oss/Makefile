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

CFLAGS+=	-I${SYSROOT:U${DESTDIR}}/${INCLUDEDIR}/private/samplerate

LIBADD=		cuse m nv pthread samplerate
# The --export-dynamic-symbol flags below are needed because some backends make
# use of those symbols.
LDFLAGS+=	-Wl,--export-dynamic-symbol=virtual_oss_wait \
		-Wl,--export-dynamic-symbol=voss_has_synchronization

.include <bsd.prog.mk>
