# $FreeBSD$

PROG=		ld-elf.so.1
SRCS=		rtld_start.S rtld.c rtld_lock.c map_object.c malloc.c \
		xmalloc.c debug.c reloc.c
MAN=		rtld.1
CSTD?=		gnu99
CFLAGS+=	-Wall -DFREEBSD_ELF -DIN_RTLD
CFLAGS+=	-I${.CURDIR}/${MACHINE_ARCH} -I${.CURDIR}
LDFLAGS+=	-nostdlib -e .rtld_start
INSTALLFLAGS=	-fschg -C -b
BINDIR=		/libexec
SYMLINKS=	${BINDIR}/${PROG} /usr/libexec/${PROG}
MLINKS=		rtld.1 ld-elf.so.1.1 \
		rtld.1 ld.so.1

#
# To enable the libmap.conf functionality please
# add 'WITH_LIBMAP=yes' to /etc/make.conf, recompile
# and reinstall rtld-elf.
.ifdef WITH_LIBMAP
CFLAGS+=	-DWITH_LIBMAP
SRCS+=		libmap.c
.endif

.if exists(${.CURDIR}/${MACHINE_ARCH}/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/Makefile.inc"
.endif

# If LDSCRIPT is defined, we build the dynamic linker as an
# executable.  Otherwise we build it as a shared object.  We ignore
# LDSCRIPT if the running kernel is too old to support it.
.if defined(LDSCRIPT)
KERN_OSRELDATE!=	/sbin/sysctl -n kern.osreldate 2>/dev/null || echo 0
.if ${KERN_OSRELDATE} >= 400001
LDSO_IS_EXECUTABLE=	yes
.endif
.endif

.ifdef LDSO_IS_EXECUTABLE
OBJS+=		dyn_hack.so
LDFLAGS+=	-Wl,-T,${LDSCRIPT} -Wl,-E -Wl,-Bstatic
DPADD=		${LIBC}
LDADD=		-lc
.else
CFLAGS+=	-fpic -DPIC
LDFLAGS+=	-shared -Wl,-Bsymbolic
DPADD=		${LIBC_PIC}
LDADD=		-lc_pic
.endif

dyn_hack.so:
	${CC} -shared -nostdlib -o dyn_hack.so -x c /dev/null

# Since moving rtld-elf to /libexec, we need to create a symlink.
# Fixup the existing binary that's there so we can symlink over it.
beforeinstall:
	-chflags noschg ${DESTDIR}/usr/libexec/${PROG}

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.prog.mk>
