# $FreeBSD$

.include <bsd.own.mk>

LIB=	procstat

SRCS=	cd9660.c	\
	common_kvm.c	\
	core.c		\
	libprocstat.c	\
	msdosfs.c	\
	smbfs.c		\
	udf.c

VERSION_DEF=	${SRCTOP}/lib/libc/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map

INCS=		libprocstat.h
CFLAGS+=	-I. -I${.CURDIR} -D_KVM_VNODE
SHLIB_MAJOR=	1

DPADD=		${LIBELF} ${LIBKVM} ${LIBUTIL}
LDADD=		-lelf -lkvm -lutil

MAN=		libprocstat.3

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
