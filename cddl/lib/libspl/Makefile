# $FreeBSD$

.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libspl
.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libspl/os/freebsd
.PATH: ${SRCTOP}/sys/contrib/openzfs/include


LIB=	spl
LIBADD=
PACKAGE=	runtime

SRCS = \
        assert.c \
        list.c \
        mkdirp.c \
        page.c \
        timestamp.c \
        zone.c \
        include/sys/list.h \
        include/sys/list_impl.h

# These functions are not required when bootstrapping and the atomic code
# will not compile when building on macOS.
.if !defined(BOOTSTRAPPING)
SRCS += \
        getexecname.c \
        gethostid.c \
        getmntany.c \
        mnttab.c


.if ${MACHINE_ARCH} == "amd64"
.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libspl/asm-x86_64
SRCS += atomic.S
.elif ${MACHINE_ARCH} == "i386"
.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libspl/asm-i386
SRCS += atomic.S
.else
.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libspl/asm-generic
SRCS += atomic.c
.endif
.endif


WARNS?=	2
CSTD=	c99
CFLAGS+= -DIN_BASE
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/module/icp/include
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h
CFLAGS.atomic.S+= -DLOCORE

.include <bsd.lib.mk>
