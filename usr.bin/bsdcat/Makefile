.include <src.opts.mk>

.include "${SRCTOP}/lib/libarchive/Makefile.inc"

PROG=	bsdcat

.PATH:	${_LIBARCHIVEDIR}/cat
SRCS=	bsdcat.c cmdline.c

.PATH:	${_LIBARCHIVEDIR}/libarchive_fe
SRCS+=	lafe_err.c

LIBADD=	archive

CFLAGS+= -DBSDCAT_VERSION_STRING=\"${LIBARCHIVE_VERSION_ONLY_STRING}\"
CFLAGS+= -I${_LIBARCHIVEDIR}/cat

#SYMLINKS=bsdcat ${BINDIR}/cat
#MLINKS= bsdcat.1 cat.1

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
