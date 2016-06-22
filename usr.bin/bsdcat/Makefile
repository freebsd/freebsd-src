# $FreeBSD$

.include <src.opts.mk>

_LIBARCHIVEDIR=	${.CURDIR}/../../contrib/libarchive
_LIBARCHIVECONFDIR=	${.CURDIR}/../../lib/libarchive

PROG=	bsdcat	
BSDCAT_VERSION_STRING=	3.2.1

.PATH:	${_LIBARCHIVEDIR}/cat
SRCS=	bsdcat.c cmdline.c

.PATH:	${_LIBARCHIVEDIR}/libarchive_fe
SRCS+=	err.c

CFLAGS+= -DBSDCAT_VERSION_STRING=\"${BSDCAT_VERSION_STRING}\"
CFLAGS+= -DPLATFORM_CONFIG_H=\"${_LIBARCHIVECONFDIR}/config_freebsd.h\"
CFLAGS+= -I${_LIBARCHIVEDIR}/cat -I${_LIBARCHIVEDIR}/libarchive_fe

LIBADD=	archive

.if ${MK_ICONV} != "no"
CFLAGS+=	-DHAVE_ICONV=1 -DHAVE_ICONV_H=1 -DICONV_CONST=const
.endif

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
