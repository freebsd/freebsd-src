# $FreeBSD$
# $P4: //depot/projects/trustedbsd/capabilities/src/libexec/rtld-elf-cap/Makefile#21 $

WITHOUT_SSP=

.include <bsd.own.mk>

PROG?=		ld-elf-cap.so.1
SRCS=		rtld_start.S \
		reloc.c rtld.c rtld_lock.c map_object.c \
		malloc.c xmalloc.c debug.c \
		crtbrand.c rtld_sandbox.c
MAN=		rtld-elf-cap.1
CSTD?=		gnu99
CFLAGS+=	-Wall -DFREEBSD_ELF -DIN_RTLD -DIN_RTLD_CAP -g
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../rtld-elf/${MACHINE_ARCH} -I${.CURDIR}/../rtld-elf
LDFLAGS+=	-nostdlib -Wl,-e,.rtld_start
INSTALLFLAGS=	-C -b
PRECIOUSPROG=
BINDIR=		/libexec
MLINKS=		rtld-elf-cap.1 ld-elf-cap.so.1.1

CFLAGS+=	-fpic -DPIC
LDFLAGS+=	-shared -Wl,-Bsymbolic
DPADD=		${LIBC_PIC}
LDADD=		-lc_pic

.if ${MACHINE_ARCH} != "ia64"
.if ${MK_SYMVER} == "yes"
LIBCDIR=	${.CURDIR}/../../lib/libc
VERSION_DEF=	${LIBCDIR}/Versions.def
SYMBOL_MAPS=	${.CURDIR}/../rtld-elf/Symbol.map
VERSION_MAP=	Version.map
LDFLAGS+=	-Wl,--version-script=${VERSION_MAP}

${PROG}:	${VERSION_MAP}

.if exists(${.CURDIR}/../rtld-elf/${MACHINE_ARCH}/Symbol.map)
SYMBOL_MAPS+=	${.CURDIR}/../rtld-elf/${MACHINE_ARCH}/Symbol.map
.endif
.endif
.endif

SYMBOL_MAPS+=	${.CURDIR}/Symbol.map

.if exists(${.CURDIR}/../rtld-elf/${MACHINE_ARCH}/Makefile.inc)
.include "${.CURDIR}/../rtld-elf/${MACHINE_ARCH}/Makefile.inc"
.endif

.PATH: ${.CURDIR}/../rtld-elf
.PATH: ${.CURDIR}/../rtld-elf/${MACHINE_ARCH}
.PATH: ${.CURDIR}/../../lib/csu/common

.include <bsd.symver.mk>
.include <bsd.prog.mk>
