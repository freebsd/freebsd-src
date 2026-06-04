
.include <src.opts.mk>

.include "${SRCTOP}/lib/libarchive/Makefile.inc"

PROG=	bsdunzip

.PATH:	${_LIBARCHIVEDIR}/unzip
SRCS=	bsdunzip.c

.PATH:	${_LIBARCHIVEDIR}/libarchive_fe
SRCS+=	cmdline.c lafe_err.c lafe_getline.c passphrase.c

LIBADD=	archive

CFLAGS+= -DBSDUNZIP_VERSION_STRING=\"${LIBARCHIVE_VERSION_ONLY_STRING}\"
CFLAGS+= -I${_LIBARCHIVEDIR}/unzip

SYMLINKS=bsdunzip ${BINDIR}/unzip
MLINKS= bsdunzip.1 unzip.1

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
