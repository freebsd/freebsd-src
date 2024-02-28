
# Use the following command to build local debug version of dynamic
# linker:
# make DEBUG_FLAGS=-g WITHOUT_TESTS=yes all

RTLD_ELF_DIR:=	${.PARSEDIR}

.include <src.opts.mk>
PACKAGE=	clibs
MK_PIE=		no # Always position independent using local rules
# Not compatible with sanitizer instrumentation or SSP.
MK_ASAN=	no
MK_SSP=		no
MK_UBSAN=	no

.include <bsd.compat.pre.mk>

.if !defined(NEED_COMPAT)
CONFS=		libmap.conf
.endif
PROG?=		ld-elf.so.1
.for _libcompat in ${_ALL_libcompats}
.if ${PROG:M*ld-elf${_libcompat}[-.]*} != ""
TAGS+=		lib${_libcompat}
.endif
.endfor
SRCS= \
	crtbrand.S \
	rtld_start.S \
	reloc.c \
	rtld.c \
	rtld_lock.c \
	rtld_malloc.c \
	rtld_printf.c \
	map_object.c \
	xmalloc.c \
	debug.c \
	libmap.c
MAN?=		rtld.1
ACFLAGS+=	-DLOCORE
CFLAGS+=	-Wall -DIN_RTLD -ffreestanding
CFLAGS+=	-I${SRCTOP}/lib/csu/common
.if exists(${RTLD_ELF_DIR}/${MACHINE_ARCH:S/powerpc64le/powerpc64/})
RTLD_ARCH=	${MACHINE_ARCH:S/powerpc64le/powerpc64/}
.else
RTLD_ARCH=	${MACHINE_CPUARCH}
.endif
CFLAGS+=	-I${RTLD_ELF_DIR}/${RTLD_ARCH} -I${RTLD_ELF_DIR}

NO_WCAST_ALIGN=	yes
INSTALLFLAGS=	-C -b
PRECIOUSPROG=
BINDIR=		/libexec
SYMLINKS=	../..${BINDIR}/${PROG} ${LIBEXECDIR}/${PROG}
MLINKS?=	rtld.1 ld-elf.so.1.1 \
		rtld.1 ld.so.1

CFLAGS+=	-fpic -DPIC $(DEBUG)

LDFLAGS+=	-shared -Wl,-Bsymbolic -Wl,-z,defs -nostdlib -e ${RTLD_ENTRY}
# Pull in the dependencies that we use from libc
.include "rtld-libc/Makefile.inc"

VERSION_DEF=	${LIBCSRCDIR}/Versions.def
SYMBOL_MAPS=	${RTLD_ELF_DIR}/Symbol.map
VERSION_MAP=	Version.map
LDFLAGS+=	-Wl,--version-script=${VERSION_MAP}

.if exists(${RTLD_ELF_DIR}/${RTLD_ARCH}/Symbol.map)
SYMBOL_MAPS+=	${RTLD_ELF_DIR}/${RTLD_ARCH}/Symbol.map
.endif

.sinclude "${RTLD_ELF_DIR}/${RTLD_ARCH}/Makefile.inc"
RTLD_ENTRY?=	.rtld_start

# Always produce the map file so that may be inspected to confirm
# undesired code is not linked from libsys/libc.
MAPFILE=	ld-elf.so.1.map
LDFLAGS+=	-Wl,-Map=${MAPFILE} -Wl,--cref
CLEANFILES+=	${MAPFILE}

afterbuild:
	@if grep __libsys_interposing ${MAPFILE} >/dev/null ; then \
		echo "libsys_interposing leaked" 1>&2 ; \
		exit 1 ; \
	fi
	@if grep __libc_interposing ${MAPFILE} >/dev/null ; then \
		echo "libc_interposing leaked" 1>&2 ; \
		exit 1 ; \
	fi
	@if grep xlocale ${MAPFILE} >/dev/null ; then \
		echo "xlocale leaked" 1>&2 ; \
		exit 1 ; \
	fi
	@if grep fprintf ${MAPFILE} >/dev/null ; then \
		echo "stdio leaked" 1>&2 ; \
		exit 1 ; \
	fi


# Since moving rtld-elf to /libexec, we need to create a symlink.
# Fixup the existing binary that's there so we can symlink over it.
beforeinstall:
.if exists(${DESTDIR}/usr/libexec/${PROG}) && ${MK_STAGING} == "no"
	-chflags -h noschg ${DESTDIR}/usr/libexec/${PROG}
.endif

.PATH: ${RTLD_ELF_DIR}/${RTLD_ARCH} ${SRCTOP}/lib/csu/common

.if ${.CURDIR} == ${RTLD_ELF_DIR}
HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests
.endif

# Some of the required math functions (div & mod) are implemented in
# libcompiler_rt on some architectures.
LIBADD+=	compiler_rt

.include <bsd.prog.mk>
${PROG_FULL}:	${VERSION_MAP}
.include <bsd.symver.mk>

.if ${COMPILER_TYPE} == "gcc"
# GCC warns about redeclarations even though they have __exported
# and are therefore not identical to the ones from the system headers.
CFLAGS+=	-Wno-redundant-decls
.endif
