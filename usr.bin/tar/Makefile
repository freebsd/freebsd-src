# $FreeBSD$
.include <bsd.own.mk>

LIBARCHIVEDIR=	${.CURDIR}/../../contrib/libarchive

PROG=	bsdtar
BSDTAR_VERSION_STRING=2.8.5

.PATH: ${LIBARCHIVEDIR}/tar
SRCS=	bsdtar.c	\
	cmdline.c	\
	getdate.c	\
	read.c		\
	subst.c		\
	tree.c		\
	util.c		\
	write.c

.PATH: ${LIBARCHIVEDIR}/libarchive_fe
SRCS+=	err.c		\
	line_reader.c	\
	matching.c	\
	pathmatch.c

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
CFLAGS+=	-DPLATFORM_CONFIG_H=\"${.CURDIR}/config_freebsd.h\"
CFLAGS+=	-I${LIBARCHIVEDIR}/tar -I${LIBARCHIVEDIR}/libarchive
CFLAGS+=	-I${LIBARCHIVEDIR}/libarchive_fe
SYMLINKS=	bsdtar ${BINDIR}/tar
MLINKS=	bsdtar.1 tar.1

.PHONY: check test clean-test
check test: $(PROG) bsdtar.1.gz
	cd ${.CURDIR}/test && make obj && make test

clean-test:
	cd ${.CURDIR}/test && make clean

.include <bsd.prog.mk>
