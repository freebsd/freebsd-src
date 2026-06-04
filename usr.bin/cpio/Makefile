
.include <src.opts.mk>

.include "${SRCTOP}/lib/libarchive/Makefile.inc"

PROG=	bsdcpio

.PATH:	${_LIBARCHIVEDIR}/cpio
SRCS=	cpio.c cmdline.c

.PATH:	${_LIBARCHIVEDIR}/libarchive_fe
SRCS+=	lafe_err.c line_reader.c passphrase.c

LIBADD=	archive

CFLAGS+=	-DBSDCPIO_VERSION_STRING=\"${LIBARCHIVE_VERSION_ONLY_STRING}\"
CFLAGS+=	-I${_LIBARCHIVEDIR}/tar

SYMLINKS=	bsdcpio ${BINDIR}/cpio
MLINKS=	bsdcpio.1 cpio.1

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
