# $FreeBSD$

.include <bsd.own.mk>
MK_SSP=		no

PROG?=		ld-elf.so.1
SRCS=		rtld_start.S \
		reloc.c rtld.c rtld_lock.c map_object.c \
		malloc.c xmalloc.c debug.c libmap.c
MAN=		rtld.1
CSTD?=		gnu99
CFLAGS+=	-Wall -DFREEBSD_ELF -DIN_RTLD
CFLAGS+=	-I${.CURDIR}/${MACHINE_ARCH} -I${.CURDIR}
LDFLAGS+=	-nostdlib -e .rtld_start
WARNS?=		2
INSTALLFLAGS=	-C -b
PRECIOUSPROG=
BINDIR=		/libexec
SYMLINKS=	${BINDIR}/${PROG} /usr/libexec/${PROG}
MLINKS=		rtld.1 ld-elf.so.1.1 \
		rtld.1 ld.so.1

CFLAGS+=	-fpic -DPIC
LDFLAGS+=	-shared -Wl,-Bsymbolic
DPADD=		${LIBC_PIC}
LDADD=		-lc_pic -lssp_nonshared

.if ${MACHINE_ARCH} != "ia64"
.if ${MK_SYMVER} == "yes"
LIBCDIR=	${.CURDIR}/../../lib/libc
VERSION_DEF=	${LIBCDIR}/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map
VERSION_MAP=	Version.map
LDFLAGS+=	-Wl,--version-script=${VERSION_MAP}

${PROG}:	${VERSION_MAP}

.if exists(${.CURDIR}/${MACHINE_ARCH}/Symbol.map)
SYMBOL_MAPS+=	${.CURDIR}/${MACHINE_ARCH}/Symbol.map
.endif
.endif
.endif

.if exists(${.CURDIR}/${MACHINE_ARCH}/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/Makefile.inc"
.endif

# Since moving rtld-elf to /libexec, we need to create a symlink.
# Fixup the existing binary that's there so we can symlink over it.
beforeinstall:
.if exists(${DESTDIR}/usr/libexec/${PROG})
	-chflags noschg ${DESTDIR}/usr/libexec/${PROG}
.endif

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.symver.mk>
.include <bsd.prog.mk>
