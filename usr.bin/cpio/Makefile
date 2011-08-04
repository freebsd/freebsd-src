# $FreeBSD$

.include <bsd.own.mk>

PROG=	bsdcpio
BSDCPIO_VERSION_STRING=2.8.4

SRCS=	cpio.c cmdline.c

.PATH:	${.CURDIR}/../../lib/libarchive/libarchive_fe
SRCS+=	err.c line_reader.c matching.c pathmatch.c

WARNS?=	6
DPADD=	${LIBARCHIVE} ${LIBZ} ${LIBBZ2}
CFLAGS+= -DBSDCPIO_VERSION_STRING=\"${BSDCPIO_VERSION_STRING}\"
CFLAGS+= -DPLATFORM_CONFIG_H=\"config_freebsd.h\"
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../../lib/libarchive/libarchive_fe

.ifdef RELEASE_CRUNCH
# FreeBSD's installer uses cpio in crunched binaries that are
# statically linked, cannot use -lcrypto, and are size sensitive.
CFLAGS+= -DSMALLER
.endif
DPADD=	${LIBARCHIVE} ${LIBZ} ${LIBBZ2} ${LIBLZMA} ${LIBBSDXML}
LDADD=	-larchive -lz -lbz2 -llzma -lbsdxml
.if ${MK_OPENSSL} != "no"
DPADD+=	${LIBCRYPTO}
LDADD+= -lcrypto
.else
DPADD+= ${LIBMD}
LDADD+= -lmd
.endif

SYMLINKS=bsdcpio ${BINDIR}/cpio
MLINKS= bsdcpio.1 cpio.1

.PHONY: check test

check test: $(PROG) bsdcpio.1.gz
	cd ${.CURDIR}/test && make clean test

.include <bsd.prog.mk>
