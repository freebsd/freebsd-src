SHLIB_NAME= 	voss_null.so
SHLIBDIR=	${LIBDIR}/virtual_oss

SRCS=		null.c

CFLAGS+= 	-I${SRCTOP}/usr.sbin/virtual_oss/virtual_oss \
		-I${SRCTOP}/contrib/libsamplerate
LIBADD=		samplerate

.include <bsd.lib.mk>
