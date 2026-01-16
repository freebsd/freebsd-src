SHLIB_NAME= 	voss_sndio.so

SRCS=		sndio.c

CFLAGS+= 	-I${SYSROOT:U${DESTDIR}}/${INCLUDEDIR}/private/samplerate \
		-I${LOCALBASE:U/usr/local}/include
LDFLAGS+=	-L${LOCALBASE:U/usr/local}/lib -lsndio
LIBADD=		samplerate

.include <bsd.lib.mk>
