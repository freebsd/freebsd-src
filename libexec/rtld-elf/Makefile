# $FreeBSD$

PROG=		ld-elf.so.1
SRCS=		rtld_start.S rtld.c lockdflt.c map_object.c malloc.c \
		xmalloc.c debug.c reloc.c
MAN=		rtld.1
CFLAGS+=	-Wall -DFREEBSD_ELF -I${.CURDIR}/${MACHINE_ARCH} -I${.CURDIR}
LDFLAGS+=	-nostdlib -e .rtld_start
INSTALLFLAGS=	-fschg -C -b
MLINKS=		rtld.1 ld-elf.so.1.1 \
		rtld.1 ld.so.1

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

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.prog.mk>
