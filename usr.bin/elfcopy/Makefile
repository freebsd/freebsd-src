.include <src.opts.mk>

PACKAGE=	toolchain

ELFTCDIR=	${SRCTOP}/contrib/elftoolchain
ELFCOPYDIR=	${ELFTCDIR}/elfcopy

.PATH: ${ELFCOPYDIR}

PROG=	elfcopy
MAN=	elfcopy.1

.if ${MK_LLVM_BINUTILS} == "no"

LINKS+=	${BINDIR}/${PROG} ${BINDIR}/objcopy
LINKS+=	${BINDIR}/${PROG} ${BINDIR}/strip
MAN+=	objcopy.1 strip.1

objcopy.1: elfcopy.1
	sed -e 's/\.Dt ELFCOPY 1/.Dt OBJCOPY 1/' \
	    -e '/\.Nm elfcopy ,/d' < ${.ALLSRC} > ${.TARGET}
CLEANFILES+= objcopy.1

.endif

SRCS=	archive.c ascii.c binary.c main.c pe.c sections.c segments.c symbols.c

WARNS?=	5

LIBADD=	archive elftc elf pe

CFLAGS+=-I${ELFTCDIR}/libelftc -I${ELFTCDIR}/libpe -I${ELFTCDIR}/common
CFLAGS+=-DWITH_PE=1



# This same hack is in lib/libelf/Makefile and usr.bin/readelf/Makefile
# We need to link against the correct version of these files. One
# solution is to include SRCTOP/sys in the include path. This causes
# problems when a header file in sys depends on a file in another
# part of the tree, e.g. a machine dependent header.
#
SRCS+=		sys/elf_common.h
CLEANDIRS=	sys
CFLAGS+=	-I.
sys/elf_common.h: ${SRCTOP}/sys/${.TARGET} .NOMETA
	mkdir -p ${.OBJDIR}/sys
	ln -sf ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
