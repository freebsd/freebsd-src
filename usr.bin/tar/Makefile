# $FreeBSD$
.include <bsd.own.mk>

PROG=	bsdtar
BSDTAR_VERSION_STRING=2.7.0
SRCS=	bsdtar.c	\
	cmdline.c	\
	err.c		\
	getdate.c	\
	line_reader.c	\
	matching.c	\
	pathmatch.c	\
	read.c		\
	subst.c		\
	tree.c		\
	util.c		\
	write.c
DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ} ${LIBMD}
LDADD=	-larchive -lbz2 -lz -lmd
.if ${MK_OPENSSL} != "no"
DPADD+=	${LIBCRYPTO}
LDADD+= -lcrypto
.endif
CFLAGS+=	-DBSDTAR_VERSION_STRING=\"${BSDTAR_VERSION_STRING}\"
CFLAGS+=	-DPLATFORM_CONFIG_H=\"config_freebsd.h\"
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../../lib/libarchive
SYMLINKS=	bsdtar ${BINDIR}/tar
MLINKS=	bsdtar.1 tar.1
DEBUG_FLAGS=-g

.PHONY: check test
check test: $(PROG) bsdtar.1.gz
	cd ${.CURDIR}/test && make test

.include <bsd.prog.mk>
