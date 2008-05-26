# $FreeBSD$

PROG=	bsdcpio
BSDCPIO_VERSION_STRING=0.9.11b
SRCS=	cpio.c cmdline.c err.c matching.c pathmatch.c
WARNS?=	6
DPADD=	${LIBARCHIVE} ${LIBZ} ${LIBBZ2}
CFLAGS+= -DBSDCPIO_VERSION_STRING=\"${BSDCPIO_VERSION_STRING}\"
CFLAGS+= -DPLATFORM_CONFIG_H=\"config_freebsd.h\" -O0 -g
LDADD+=	-larchive -lz -lbz2
SYMLINKS=bsdcpio ${BINDIR}/cpio
MLINKS= bsdcpio.1 cpio.1

.PHONY: check test

check test: $(PROG) bsdcpio.1.gz
	cd ${.CURDIR}/test && make clean test

.include <bsd.prog.mk>
