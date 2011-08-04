# $FreeBSD$
.include <bsd.own.mk>

PROG=	bsdtar
BSDTAR_VERSION_STRING=2.8.4
SRCS=	bsdtar.c	\
	cmdline.c	\
	getdate.c	\
	read.c		\
	subst.c		\
	tree.c		\
	util.c		\
	write.c

.PATH: ${.CURDIR}/../../lib/libarchive/libarchive_fe
SRCS+=	err.c		\
	line_reader.c	\
	matching.c	\
	pathmatch.c

WARNS?=	6
DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ} ${LIBLZMA} ${LIBBSDXML}
LDADD=	-larchive -lbz2 -lz -llzma -lbsdxml
.if ${MK_OPENSSL} != "no"
DPADD+=	${LIBCRYPTO}
LDADD+= -lcrypto
.else
DPADD+= ${LIBMD}
LDADD+= -lmd
.endif

CFLAGS+=	-DBSDTAR_VERSION_STRING=\"${BSDTAR_VERSION_STRING}\"
CFLAGS+=	-DPLATFORM_CONFIG_H=\"config_freebsd.h\"
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../../lib/libarchive
CFLAGS+=	-I${.CURDIR}/../../lib/libarchive/libarchive_fe
SYMLINKS=	bsdtar ${BINDIR}/tar
MLINKS=	bsdtar.1 tar.1
DEBUG_FLAGS=-g

.PHONY: check test
check test: $(PROG) bsdtar.1.gz
	cd ${.CURDIR}/test && make test

.include <bsd.prog.mk>
