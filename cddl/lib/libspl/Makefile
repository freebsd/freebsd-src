.include <bsd.init.mk>
.include <bsd.compiler.mk>

.PATH: ${ZFSTOP}/lib/libspl
.PATH: ${ZFSTOP}/include

PACKAGE=	zfs
LIB_PACKAGE=

LIB=	spl
LIBADD=

SRCS = \
        assert.c \
	backtrace.c \
	condvar.c \
	kmem.c \
	libspl.c \
        list.c \
        mkdirp.c \
        mutex.c \
        os/freebsd/zone.c \
        page.c \
	procfs_list.c \
	random.c \
	rwlock.c \
	sid.c \
	taskq.c \
        timestamp.c \
        include/sys/list.h \
        include/sys/list_impl.h

# These functions are not required when bootstrapping and the atomic code,
# among others, will not compile when building on macOS.
.if !defined(BOOTSTRAPPING)
SRCS += \
        atomic.c \
	cred.c \
        getexecname.c \
	kstat.c \
	thread.c \
        tunables.c \
        os/freebsd/getexecname.c \
        os/freebsd/gethostid.c \
        os/freebsd/getmntany.c \
        os/freebsd/mnttab.c
.endif

WARNS?=	2
CSTD=	c99
CFLAGS+= -DIN_BASE
CFLAGS+= -I${ZFSTOP}/include
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${ZFSTOP}/module/icp/include
CFLAGS+= -include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h
.if ${COMPILER_TYPE} == "clang"
CFLAGS.atomic.c+=	-Wno-error=atomic-alignment
.endif

.include <bsd.lib.mk>
