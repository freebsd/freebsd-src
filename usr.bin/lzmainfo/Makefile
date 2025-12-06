PACKAGE=xz

PROG=	lzmainfo

XZDIR=	${SRCTOP}/contrib/xz/src
LZMALIBDIR=	${SRCTOP}/lib/liblzma

.PATH: ${XZDIR}/lzmainfo
SRCS+=	lzmainfo.c

.PATH: ${XZDIR}/common
SRCS+=	tuklib_progname.c \
	tuklib_exit.c \
	tuklib_mbstr_nonprint.c \
	tuklib_mbstr_width.c \
	tuklib_mbstr_wrap.c

CFLAGS+=	-DHAVE_CONFIG_H \
		-I${LZMALIBDIR} \
		-I${XZDIR}/common

LIBADD=	lzma

.include <bsd.prog.mk>
