SHLIB_NAME= 	voss_bt.so

SRCS=		bt.c \
		avdtp.c \
		sbc_encode.c

LIBADD=		bluetooth sdp

.if defined(HAVE_LIBAV)
CFLAGS+=	-I${LOCALBASE:U/usr/local}/include -DHAVE_LIBAV
LDFLAGS+= 	-L${LOCALBASE:U/usr/local}/lib \
		-lavdevice -lavutil -lavcodec -lavformat
.endif

.include <bsd.lib.mk>
