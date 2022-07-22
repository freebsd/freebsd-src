# $FreeBSD$

.include <bsd.init.mk>
.include <bsd.compiler.mk>

.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libspl
.PATH: ${SRCTOP}/sys/contrib/openzfs/include

LIB=	spl
LIBADD=
PACKAGE=	zfs

SRCS = \
        assert.c \
        list.c \
        mkdirp.c \
	os/freebsd/zone.c \
        page.c \
        timestamp.c \
        include/sys/list.h \
        include/sys/list_impl.h

# These functions are not required when bootstrapping and the atomic code
# will not compile when building on macOS.
.if !defined(BOOTSTRAPPING)
SRCS += \
	atomic.c \
	os/freebsd/getexecname.c \
	os/freebsd/gethostid.c \
	os/freebsd/getmntany.c \
	os/freebsd/mnttab.c
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
.if ${COMPILER_TYPE} == "clang"
CFLAGS.atomic.c+=	-Wno-error=atomic-alignment
.endif

.include <bsd.lib.mk>
