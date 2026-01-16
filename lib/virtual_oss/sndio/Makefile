SHLIB_NAME= 	voss_sndio.so

SRCS=		sndio.c

CFLAGS+=	-I${LOCALBASE:U/usr/local}/include \
LDFLAGS+=	-L${LOCALBASE:U/usr/local}/lib -lsndio

.include <bsd.lib.mk>
