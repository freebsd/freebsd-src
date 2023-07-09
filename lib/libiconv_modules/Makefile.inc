# $FreeBSD$

.PATH: ${SRCTOP}/lib/libc/iconv

SHLIB_MAJOR= 5
CFLAGS+= -I${SRCTOP}/lib/libc/iconv

CFLAGS+=	-Dbool=_Bool

SHLIBDIR= /usr/lib${COMPAT_libcompat:U}/i18n
LIBDIR=	 ${SHLIBDIR}
MK_PROFILE=	no
