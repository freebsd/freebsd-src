# $FreeBSD: src/libexec/rtld-elf/Makefile,v 1.10.2.3 2000/07/20 11:16:05 obrien Exp $

MAINTAINER=	jdp
PROG=		ld-elf.so.1
SRCS=		rtld_start.S rtld.c lockdflt.c map_object.c malloc.c \
		xmalloc.c debug.c reloc.c
MAN1=		rtld.1
CFLAGS+=	-Wall -DFREEBSD_ELF -I${.CURDIR}/${MACHINE_ARCH} -I${.CURDIR}
LDFLAGS+=	-nostdlib -e .rtld_start
INSTALLFLAGS+=	-fschg -C
MLINKS=		rtld.1 ld-elf.so.1.1

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

# Keep the previous version of the dynamic linker for recovery
# purposes.  This must come after the include of <bsd.prog.mk> so that
# BINDIR is set for the exists(...) constructs.
beforeinstall:
.if exists(${DESTDIR}${BINDIR}/${PROG})
	-chflags noschg ${DESTDIR}${BINDIR}/${PROG}
.if exists(${DESTDIR}${BINDIR}/${PROG}.old)
	-chflags noschg ${DESTDIR}${BINDIR}/${PROG}.old
.endif
	${INSTALL} -C -p ${DESTDIR}${BINDIR}/${PROG} \
	    ${DESTDIR}${BINDIR}/${PROG}.old
.endif
