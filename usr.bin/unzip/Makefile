
.include <src.opts.mk>

_LIBARCHIVEDIR=	${SRCTOP}/contrib/libarchive
_LIBARCHIVECONFDIR=	${SRCTOP}/lib/libarchive

PROG=	bsdunzip

BSDUNZIP_VERSION_STRING!= sed -n '/define.*ARCHIVE_VERSION_ONLY_STRING/{s,[^0-9.],,gp;q;}' \
			${_LIBARCHIVEDIR}/libarchive/archive.h

.PATH:	${_LIBARCHIVEDIR}/unzip
SRCS=	bsdunzip.c

.PATH:	${_LIBARCHIVEDIR}/libarchive_fe
SRCS+=	cmdline.c err.c passphrase.c

CFLAGS+= -DBSDUNZIP_VERSION_STRING=\"${BSDUNZIP_VERSION_STRING}\"
CFLAGS+= -DPLATFORM_CONFIG_H=\"${_LIBARCHIVECONFDIR}/config_freebsd.h\"
CFLAGS+= -I${_LIBARCHIVEDIR}/unzip -I${_LIBARCHIVEDIR}/libarchive_fe

LIBADD=	archive

SYMLINKS=bsdunzip ${BINDIR}/unzip
MLINKS= bsdunzip.1 unzip.1

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
# DO NOT DELETE
