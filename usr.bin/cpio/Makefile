# $FreeBSD$

.include <src.opts.mk>

LIBARCHIVEDIR=	${.CURDIR}/../../contrib/libarchive
LIBARCHIVECONFDIR=	${.CURDIR}/../../lib/libarchive

PROG=	bsdcpio
BSDCPIO_VERSION_STRING=	3.1.2

.PATH:	${LIBARCHIVEDIR}/cpio
SRCS=	cpio.c cmdline.c

.PATH:	${LIBARCHIVEDIR}/libarchive_fe
SRCS+=	err.c line_reader.c

CFLAGS+= -DBSDCPIO_VERSION_STRING=\"${BSDCPIO_VERSION_STRING}\"
CFLAGS+= -DPLATFORM_CONFIG_H=\"${LIBARCHIVECONFDIR}/config_freebsd.h\"
CFLAGS+= -I${LIBARCHIVEDIR}/cpio -I${LIBARCHIVEDIR}/libarchive_fe

.ifdef RELEASE_CRUNCH
# FreeBSD's installer uses cpio in crunched binaries that are
# statically linked, cannot use -lcrypto, and are size sensitive.
CFLAGS+= -DSMALLER
.endif
LIBADD=	archive

.if ${MK_ICONV} != "no"
CFLAGS+=	-DHAVE_ICONV=1 -DHAVE_ICONV_H=1 -DICONV_CONST=const
.endif

SYMLINKS=bsdcpio ${BINDIR}/cpio
MLINKS= bsdcpio.1 cpio.1

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
