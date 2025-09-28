SHLIB_NAME= 	voss_sndio.so
SHLIBDIR=	${LIBDIR}/virtual_oss

SRCS=		sndio.c

CFLAGS+= 	-I${SRCTOP}/usr.sbin/virtual_oss/virtual_oss \
		-I${SRCTOP}/contrib/libsamplerate \
		-I${LOCALBASE:U/usr/local}/include
LDFLAGS+=	-L${LOCALBASE:U/usr/local}/lib -lsndio
LIBADD=		samplerate

.include <bsd.lib.mk>
