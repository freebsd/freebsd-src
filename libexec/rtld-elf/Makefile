#
#	$Id: Makefile,v 1.5 1999/02/15 05:02:54 nate Exp $
#

PROG=		ld-elf.so.1
SRCS=		rtld_start.S rtld.c map_object.c malloc.c xmalloc.c debug.c \
		reloc.c
NOMAN=		true
CFLAGS+=	-fpic -Wall -DFREEBSD_ELF -I${.CURDIR}
LDADD+=		-lc_pic


.if	${MACHINE_ARCH} == "alpha"
CFLAGS+=	-mno-fp-regs
LDFLAGS+=	-nostdlib -Wl,-Bshareable,-Bsymbolic -e .rtld_start
.elif	${MACHINE_ARCH} == "i386"
CFLAGS+=	-elf
LDFLAGS+=	-elf -nostdlib -Wl,-Bshareable,-Bsymbolic
.endif

# Atomic installation with "-C" is very important for this program.
INSTALLFLAGS+=	-fschg -C

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <bsd.prog.mk>
