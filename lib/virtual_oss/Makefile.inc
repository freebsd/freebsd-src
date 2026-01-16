PACKAGE=	sound

SHLIBDIR=	${LIBDIR}/virtual_oss

CFLAGS+=	-I${SRCTOP}/usr.sbin/virtual_oss/virtual_oss
# XXX virtual_oss/int.h includes samplerate.h, but the library isn't used
CFLAGS+=	-I${SYSROOT:U${DESTDIR}}/${INCLUDEDIR}/private/samplerate

.include "../Makefile.inc"
