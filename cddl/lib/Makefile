# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	libavl \
	libnvpair \
	libumem \
	libuutil \
	${_libzfs} \
	${_libzpool}

.if ${MK_ZFS} != "no"
_libzfs=	libzfs
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
_libzpool=	libzpool
.endif
.endif

.include <bsd.subdir.mk>
