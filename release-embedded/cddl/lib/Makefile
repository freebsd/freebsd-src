# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	${_drti} \
	libavl \
	libctf \
	${_libdtrace} \
	libnvpair \
	libumem \
	libuutil \
	${_libzfs_core} \
	${_libzfs} \
	${_libzpool}

.if ${MK_ZFS} != "no"
_libzfs_core=	libzfs_core
_libzfs=	libzfs
.if ${MK_LIBTHR} != "no"
_libzpool=	libzpool
.endif
.endif

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386" || \
	${MACHINE_CPUARCH} == "mips" || ${MACHINE_CPUARCH} == "powerpc"
_drti=		drti
_libdtrace=	libdtrace
.endif

.include <bsd.subdir.mk>
