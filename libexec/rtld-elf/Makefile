#
#	$Id: Makefile,v 1.4.2.1 1999/03/24 23:39:14 nate Exp $
#
PROG=		ld-elf.so.1
SRCS=		rtld_start.S rtld.c map_object.c malloc.c xmalloc.c debug.c \
		reloc.c
NOMAN=		true
CFLAGS+=	-Wall -DFREEBSD_ELF -I${.CURDIR}/${MACHINE_ARCH} -I${.CURDIR}
LDFLAGS+=	-nostdlib -e .rtld_start
INSTALLFLAGS+=	-fschg -C

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
LDADD+=		-lc
.else
CFLAGS+=	-fpic -DPIC
LDFLAGS+=	-shared -Wl,-Bsymbolic
LDADD+=		-lc_pic
.endif

dyn_hack.so:
	${CC} -shared -nostdlib -o dyn_hack.so -x c /dev/null

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.prog.mk>
