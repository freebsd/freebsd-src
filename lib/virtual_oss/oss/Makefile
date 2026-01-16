SHLIB_NAME= 	voss_oss.so

SRCS=		oss.c

CFLAGS+= 	-I${SYSROOT:U${DESTDIR}}/${INCLUDEDIR}/private/samplerate
LIBADD=		samplerate

.include <bsd.lib.mk>
