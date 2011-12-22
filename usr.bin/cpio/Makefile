# $FreeBSD$

.include <bsd.own.mk>

LIBARCHIVEDIR=	${.CURDIR}/../../contrib/libarchive

PROG=	bsdcpio
BSDCPIO_VERSION_STRING=2.8.5

.PATH:	${LIBARCHIVEDIR}/cpio
SRCS=	cpio.c cmdline.c

.PATH:	${LIBARCHIVEDIR}/libarchive_fe
SRCS+=	err.c line_reader.c matching.c pathmatch.c

CFLAGS+= -DBSDCPIO_VERSION_STRING=\"${BSDCPIO_VERSION_STRING}\"
CFLAGS+= -DPLATFORM_CONFIG_H=\"${.CURDIR}/config_freebsd.h\"
CFLAGS+= -I${LIBARCHIVEDIR}/cpio -I${LIBARCHIVEDIR}/libarchive_fe

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

.PHONY: check test clean-test

check test: $(PROG) bsdcpio.1.gz
	cd ${.CURDIR}/test && make obj && make test

clean-test:
	cd ${.CURDIR}/test && make clean

.include <bsd.prog.mk>
