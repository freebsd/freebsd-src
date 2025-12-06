SHLIB_NAME= 	voss_bt.so
SHLIBDIR=	${LIBDIR}/virtual_oss

SRCS=		bt.c \
		avdtp.c \
		sbc_encode.c

CFLAGS+= 	-I${SRCTOP}/usr.sbin/virtual_oss/virtual_oss \
		-I${SRCTOP}/contrib/libsamplerate
LDFLAGS+=	-lbluetooth -lsdp
LIBADD=		samplerate

.if defined(HAVE_LIBAV)
CFLAGS+=	-I${LOCALBASE:U/usr/local}/include -DHAVE_LIBAV
LDFLAGS+= 	-L${LOCALBASE:U/usr/local}/lib \
		-lavdevice -lavutil -lavcodec -lavformat
.endif

.include <bsd.lib.mk>
