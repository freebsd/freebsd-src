# $FreeBSD$

#
# Use "make distfile" to build a tar.gz file suitable for distribution,
# including an autoconf/automake-generated build system.
#

PROG=	bsdtar
VERSION=	1.02.023
DIST_SRCS=	bsdtar.c getdate.y matching.c read.c tree.c util.c write.c
SRCS=	${DIST_SRCS}
WARNS?=	5
DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ}
LDADD=	-larchive -lbz2 -lz
CFLAGS+=	-DPACKAGE_VERSION=\"${VERSION}\"
CFLAGS+=	-I${.CURDIR}
SYMLINKS=	bsdtar ${BINDIR}/tar
MLINKS=	bsdtar.1 tar.1

DIST_BUILD_DIR=	${.OBJDIR}/${PROG}-${VERSION}
CLEANDIRS+=	${DIST_BUILD_DIR}
DISTFILE=	${PROG}-${VERSION}.tar.gz
# Files that just get copied to the distfile build directory
DIST_FILES=	${DIST_SRCS}
DIST_FILES+=	${MAN}
DIST_FILES+=	bsdtar.h bsdtar_platform.h
DIST_FILES+=	Makefile.am
DIST_FILES+=	tree.h

distfile:
	rm -rf ${DIST_BUILD_DIR}
	mkdir ${DIST_BUILD_DIR}
	for f in ${DIST_FILES};					\
	do							\
		cat ${.CURDIR}/$$f >${DIST_BUILD_DIR}/$$f;	\
	done
	cat ${.CURDIR}/configure.ac.in |			\
		sed 's/@VERSION@/${VERSION}/' |			\
		cat > ${DIST_BUILD_DIR}/configure.ac
	(cd ${DIST_BUILD_DIR} && aclocal && autoheader && autoconf )
	(cd ${DIST_BUILD_DIR} && automake -a --foreign)
	(cd ${DIST_BUILD_DIR} && ./configure && make distcheck && make dist)
	mv ${DIST_BUILD_DIR}/${DISTFILE} ${.OBJDIR}
	@echo ==================================================
	@echo Created ${.OBJDIR}/${DISTFILE}
	@echo ==================================================

.include <bsd.prog.mk>
