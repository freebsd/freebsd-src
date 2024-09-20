PACKAGE=clibs
LIB=dl
SHLIB_MAJOR=1

.PATH:	${SRCTOP}/lib/libc/gen
CFLAGS+=-I${SRCTOP}/lib/libc/include
RTLD_ELF_DIR=${SRCTOP}/libexec/rtld-elf
.if exists(${RTLD_ELF_DIR}/${MACHINE_ARCH:S/powerpc64le/powerpc64/})
RTLD_ARCH=	${MACHINE_ARCH:S/powerpc64le/powerpc64/}
.else
RTLD_ARCH=	${MACHINE_CPUARCH}
.endif
CFLAGS+= -I${RTLD_ELF_DIR}/${RTLD_ARCH} -I${RTLD_ELF_DIR}
CFLAGS+=-DIN_LIBDL
LDFLAGS+=-Wl,-F,libc.so.7
VERSION_DEF=${SRCTOP}/lib/libc/Versions.def
SYMBOL_MAPS=${.CURDIR}/Symbol.map

SRCS =	dlfcn.c

.include <bsd.lib.mk>
