PACKAGE=	sound

SHLIBDIR=	${LIBDIR}/virtual_oss

CFLAGS+=	-I${SRCTOP}/usr.sbin/virtual_oss/virtual_oss

.include "../Makefile.inc"
