PACKAGE=	clibs
SHLIBDIR?= /lib

.include <src.opts.mk>

LIBC_SRCTOP?= ${.CURDIR}/../libc
LIBSYS_SRCTOP?=	${.CURDIR}

# Pick the current architecture directory for libsys. In general, this is named
# MACHINE_CPUARCH, but some ABIs are different enough to require their own
# libsys, so allow a directory named MACHINE_ARCH to override this (though
# treat powerpc64le and powerpc64 the same).
# Note: This is copied from libc/Makefile
M=${MACHINE_ARCH:S/powerpc64le/powerpc64/}
.if exists(${LIBC_SRCTOP}/${M})
LIBC_ARCH=${M}
.else
LIBC_ARCH=${MACHINE_CPUARCH}
.endif

LIB=sys
SHLIB_MAJOR= 7
WARNS?=	2
MK_SSP=		no

INCS=	libsys.h _libsys.h

CFLAGS+=-I${LIBSYS_SRCTOP}/include -I${LIBC_SRCTOP}/include
CFLAGS+=-I${LIBSYS_SRCTOP}/${LIBC_ARCH}
CFLAGS+=-I${LIBC_SRCTOP}/${LIBC_ARCH}

.PATH: ${LIBC_SRCTOP}/string
SRCS+=	memcpy.c memset.c strlcpy.c

CLEANFILES+=tags
INSTALL_PIC_ARCHIVE=
#XXX? BUILD_NOSSP_PIC_ARCHIVE=
PRECIOUSLIB=

# Use a more efficient TLS model for libc since we can reasonably assume that
# it will be loaded during program startup.
CFLAGS+= -ftls-model=initial-exec

#
# Link with static libcompiler_rt.a.
#
LDFLAGS+= -nodefaultlibs
LDFLAGS+= -Wl,-Bsymbolic
LIBADD+=	compiler_rt

.if ${MK_SSP} != "no" && \
    (${LIBC_ARCH} == "i386" || ${LIBC_ARCH:Mpowerpc*} != "")
LIBADD+=        ssp_nonshared
.endif

# Define (empty) variables so that make doesn't give substitution
# errors if the included makefiles don't change these:
MDASM=
MIASM=
NOASM=

SYM_MAPS+=	${LIBSYS_SRCTOP}/Symbol.map
SRCS+=	auxv.c

.include "${LIBSYS_SRCTOP}/Makefile.sys"

SYM_MAPS+=	${LIBSYS_SRCTOP}/Symbol.thr.map
.PATH: ${LIBSYS_SRCTOP}/${MACHINE_CPUARCH}
.sinclude "${LIBSYS_SRCTOP}/${MACHINE_CPUARCH}/Makefile.thr"
.if !${SRCS:M_umtx_op_err.S}
SRCS+=_umtx_op_err.c
.endif

VERSION_DEF=${LIBC_SRCTOP}/Versions.def
SYMBOL_MAPS=${SYM_MAPS}

.include <bsd.lib.mk>
