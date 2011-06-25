# $FreeBSD$

.include <bsd.own.mk>

LIB=	procstat

SRCS=	cd9660.c	\
	common_kvm.c	\
	libprocstat.c	\
        msdosfs.c	\
	ntfs.c		\
	smbfs.c		\
	udf.c

VERSION_DEF=	${.CURDIR}/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map

INCS=	libprocstat.h
CFLAGS+=	-I. -I${.CURDIR} -D_KVM_VNODE
SHLIB_MAJOR=	1
WITHOUT_MAN=	yes
DPADD=		${LIBKVM} ${LIBUTIL}
LDADD=		-lkvm -lutil

.if ${MK_NCP} != "no"
CFLAGS+=	-DLIBPROCSTAT_NWFS
SRCS+=	nwfs.c
.endif

# XXX This is a hack.
.if ${MK_CDDL} != "no"
CFLAGS+=	-DLIBPROCSTAT_ZFS
OBJS+=	zfs/zfs.o
SOBJS+=	zfs/zfs.So
POBJS+=	zfs/zfs.po
SUBDIR=	zfs
zfs/zfs.o: .PHONY
	@cd ${.CURDIR}/zfs && ${MAKE} zfs.o
zfs/zfs.So: .PHONY
	@cd ${.CURDIR}/zfs && ${MAKE} zfs.So
zfs/zfs.po: .PHONY
	@cd ${.CURDIR}/zfs && ${MAKE} zfs.po
.endif

.include <bsd.lib.mk>
