SHLIB_NAME= 	voss_oss.so
SHLIBDIR=	${LIBDIR}/virtual_oss

SRCS=		oss.c

CFLAGS+= 	-I${SRCTOP}/usr.sbin/virtual_oss/virtual_oss \
		-I${SRCTOP}/contrib/libsamplerate
LIBADD=		samplerate

.include <bsd.lib.mk>
