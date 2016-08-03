LIB=syscalls

LIBC_SRCTOP=	${.CURDIR}/../libc/

.if exists(${LIBC_SRCTOP}/${MACHINE_ARCH})
LIBC_ARCH=${MACHINE_ARCH}
.else
LIBC_ARCH=${MACHINE_CPUARCH}
.endif

SHLIB_MAJOR= 7

CFLAGS+=-I${LIBC_SRCTOP}/include -I${LIBC_SRCTOP}/../../include
CFLAGS+=-I${LIBC_SRCTOP}/${LIBC_ARCH}

# Define (empty) variables so that make doesn't give substitution
# errors if the included makefiles don't change these:
MDSRCS=
MISRCS=
MDASM=
MIASM=
NOASM=

.include "${LIBC_SRCTOP}/sys/Makefile.inc"

VERSION_DEF=${LIBC_SRCTOP}/Versions.def
SYMBOL_MAPS=${SYM_MAPS}
CFLAGS+= -DSYMBOL_VERSIONING

MAN=

.include <bsd.lib.mk>
