# $FreeBSD$

.include <bsd.own.mk>

PROG=	bsdcpio
BSDCPIO_VERSION_STRING=1.1.0
SRCS=	cpio.c cmdline.c err.c matching.c pathmatch.c
WARNS?=	6
DPADD=	${LIBARCHIVE} ${LIBZ} ${LIBBZ2}
CFLAGS+= -DBSDCPIO_VERSION_STRING=\"${BSDCPIO_VERSION_STRING}\"
CFLAGS+= -DPLATFORM_CONFIG_H=\"config_freebsd.h\"
LDADD+=	-larchive -lz -lbz2

.if ${MK_GNU_CPIO} != "yes"
SYMLINKS=bsdcpio ${BINDIR}/cpio
MLINKS= bsdcpio.1 cpio.1
.endif

.PHONY: check test

check test: $(PROG) bsdcpio.1.gz
	cd ${.CURDIR}/test && make clean test

.include <bsd.prog.mk>
