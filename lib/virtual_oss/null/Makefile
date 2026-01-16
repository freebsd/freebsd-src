SHLIB_NAME= 	voss_null.so

SRCS=		null.c

CFLAGS+= 	-I${SYSROOT:U${DESTDIR}}/${INCLUDEDIR}/private/samplerate
LIBADD=		samplerate

.include <bsd.lib.mk>
