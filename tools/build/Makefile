# $FreeBSD$

.PATH: ${.CURDIR}/../../include

LIB=		egacy
SRC=
INCSGROUPS=	INCS SYSINCS UFSINCS FFSINCS MSDOSFSINCS DISKINCS
INCS=

SYSINCSDIR=	${INCLUDEDIR}/sys
# Also add ufs/ffs/msdosfs/disk headers to allow building makefs as a bootstrap tool
UFSINCSDIR=	${INCLUDEDIR}/ufs/ufs
FFSINCSDIR=	${INCLUDEDIR}/ufs/ffs
MSDOSFSINCSDIR=	${INCLUDEDIR}/fs/msdosfs
DISKINCSDIR=	${INCLUDEDIR}/sys/disk

BOOTSTRAPPING?=	0

_WITH_PWCACHEDB!= grep -c pwcache_groupdb /usr/include/grp.h || true
.if ${_WITH_PWCACHEDB} == 0
.PATH: ${.CURDIR}/../../contrib/libc-pwcache
CFLAGS+=	-I${.CURDIR}/../../contrib/libc-pwcache \
		-I${.CURDIR}/../../lib/libc/include
SRCS+=		pwcache.c
.endif

_WITH_STRSVIS!=	grep -c strsvis /usr/include/vis.h || true
.if ${_WITH_STRSVIS} == 0
.PATH: ${.CURDIR}/../../contrib/libc-vis
SRCS+=		vis.c
CFLAGS+=	-I${.CURDIR}/../../contrib/libc-vis \
		-I${.CURDIR}/../../lib/libc/include
.endif

_WITH_REALLOCARRAY!= grep -c reallocarray /usr/include/stdlib.h || true
.if ${_WITH_REALLOCARRAY} == 0
.PATH: ${.CURDIR}/../../lib/libc/stdlib
INCS+=		stdlib.h
SRCS+=		reallocarray.c
CFLAGS+=	-I${.CURDIR}/../../lib/libc/include
.endif

_WITH_UTIMENS!= grep -c utimensat /usr/include/sys/stat.h || true
.if ${_WITH_UTIMENS} == 0
SYSINCS+=	stat.h
SRCS+=		futimens.c utimensat.c
.endif

_WITH_EXPLICIT_BZERO!= grep -c explicit_bzero /usr/include/strings.h || true
.if ${_WITH_EXPLICIT_BZERO} == 0
.PATH: ${SRCTOP}/sys/libkern
INCS+=		strings.h
SRCS+=		explicit_bzero.c
.endif

.if empty(SRCS)
SRCS=		dummy.c
.endif

.if defined(CROSS_BUILD_TESTING)
SUBDIR=		cross-build
.endif

# To allow bootstrapping makefs on FreeBSD 11 or non-FreeBSD systems:
UFSINCS+=	${SRCTOP}/sys/ufs/ufs/dinode.h
UFSINCS+=	${SRCTOP}/sys/ufs/ufs/dir.h
FFSINCS+=	${SRCTOP}/sys/ufs/ffs/fs.h

MSDOSFSINCS+=	${SRCTOP}/sys/fs/msdosfs/bootsect.h
MSDOSFSINCS+=	${SRCTOP}/sys/fs/msdosfs/bpb.h
MSDOSFSINCS+=	${SRCTOP}/sys/fs/msdosfs/denode.h
MSDOSFSINCS+=	${SRCTOP}/sys/fs/msdosfs/direntry.h
MSDOSFSINCS+=	${SRCTOP}/sys/fs/msdosfs/fat.h
MSDOSFSINCS+=	${SRCTOP}/sys/fs/msdosfs/msdosfsmount.h
DISKINCS+=	${SRCTOP}/sys/sys/disk/bsd.h

# Needed to build config (since it uses libnv)
SYSINCS+=	${SRCTOP}/sys/sys/nv.h ${SRCTOP}/sys/sys/cnv.h


# Create all the directories that are needed during the legacy, bootstrap-tools
# and cross-tools stages. We do this here using mkdir since mtree may not exist
# yet (this happens if we are crossbuilding from Linux/Mac).
installdirs:
.for _dir in bin sbin usr/bin usr/sbin usr/lib usr/include lib/geom lib/casper
	mkdir -p "${DESTDIR}/${_dir}"
.endfor

.for _group in ${INCSGROUPS:NINCS}
	mkdir -p "${DESTDIR}/${${_group}DIR}"
.endfor

.include <bsd.lib.mk>
