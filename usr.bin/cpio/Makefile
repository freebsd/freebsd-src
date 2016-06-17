# $FreeBSD$

.include <bsd.own.mk>

LIBARCHIVEDIR=	${.CURDIR}/../../contrib/libarchive
LIBARCHIVECONFDIR=	${.CURDIR}/../../lib/libarchive

PROG=	bsdcpio
BSDCPIO_VERSION_STRING=	3.2.0

.PATH:	${LIBARCHIVEDIR}/cpio
SRCS=	cpio.c cmdline.c

.PATH:	${LIBARCHIVEDIR}/libarchive_fe
SRCS+=	err.c line_reader.c passphrase.c

CFLAGS+= -DBSDCPIO_VERSION_STRING=\"${BSDCPIO_VERSION_STRING}\"
CFLAGS+= -DPLATFORM_CONFIG_H=\"${LIBARCHIVECONFDIR}/config_freebsd.h\"
CFLAGS+= -I${LIBARCHIVEDIR}/cpio -I${LIBARCHIVEDIR}/libarchive_fe

.ifdef RELEASE_CRUNCH
# FreeBSD's installer uses cpio in crunched binaries that are
# statically linked, cannot use -lcrypto, and are size sensitive.
CFLAGS+= -DSMALLER
.endif
DPADD=	${LIBARCHIVE} ${LIBZ} ${LIBBZ2} ${LIBLZMA} ${LIBPTHREAD} ${LIBBSDXML}
LDADD=	-larchive -lz -lbz2 -llzma -lpthread -lbsdxml
.if ${MK_OPENSSL} != "no"
DPADD+=	${LIBCRYPTO}
LDADD+= -lcrypto
.else
DPADD+= ${LIBMD}
LDADD+= -lmd
.endif

.if ${MK_ICONV} != "no"
CFLAGS+=	-DHAVE_ICONV=1 -DHAVE_ICONV_H=1 -DICONV_CONST=const
.endif

SYMLINKS=bsdcpio ${BINDIR}/cpio
MLINKS= bsdcpio.1 cpio.1

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
