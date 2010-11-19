# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/bfd ${SRCDIR}/opcodes

LIB=	bfd
SRCS+=	archive.c archive64.c archures.c bfd.c bfdwin.c binary.c cache.c \
	coffgen.c corefile.c elf.c elf-eh-frame.c elf-strtab.c format.c \
	hash.c ihex.c init.c libbfd.c linker.c merge.c opncls.c reloc.c \
	section.c srec.c stab-syms.c stabs.c syms.c targets.c tekhex.c \
	targmatch.h dwarf1.c dwarf2.c config.h bfdver.h bfdio.c simple.c
WARNS?=	0
.if (${TARGET_ARCH} == "ia64" || ${TARGET_ARCH} == "sparc64")
WARNS?=	2
.endif
CFLAGS+= -D_GNU_SOURCE
CFLAGS+= -I${SRCDIR}/bfd
INTERNALLIB=
CLEANFILES+=	bfdver.h config.h targmatch.h

SELARCH=
.if ${TARGET_ARCH} == "amd64"
SELARCH= &bfd_i386_arch
.elif ${TARGET_ARCH} == "sparc64"
SELARCH= &bfd_sparc_arch
.else
.for _a in ${ARCHS}
.if ${SELARCH} == ""
SELARCH+= &bfd_${_a}_arch
.else
SELARCH+= ,&bfd_${_a}_arch
.endif
.endfor
.endif
CFLAGS+= -DSELECT_ARCHITECTURES="${SELARCH}"

SELVEC=
.for _v in ${VECS}
CFLAGS+= -DHAVE_${_v}
.if ${SELVEC} == ""
SELVEC+= &${_v}
.else
SELVEC+= ,&${_v}
.endif
.endfor
CFLAGS+= -DSELECT_VECS="${SELVEC}"
CFLAGS+= -DDEFAULT_VECTOR=${DEFAULT_VECTOR}

# XXX:DEO should grab BFD_VERSION_DATE from ${VERSION}...
bfdver.h: Makefile
	echo '#define BFD_VERSION	215000000'	> ${.TARGET}
	echo '#define BFD_VERSION_DATE	20040517'	>> ${.TARGET}
	echo '#define BFD_VERSION_STRING ${VERSION}'	>> ${.TARGET}

targmatch.h: targmatch.sed config.bfd
	sed -f ${.ALLSRC:M*.sed} ${.ALLSRC:M*.bfd} > ${.TARGET}

config.h: config.h.fbsd
.if ${TARGET_ARCH} == "i386"
	sed -e 's,!!TRAD_HEADER!!,"hosts/i386bsd.h",g' ${.ALLSRC} > ${.TARGET}
.else
	sed -e 's,!!TRAD_HEADER!!,,g' ${.ALLSRC} > ${.TARGET}
.endif

CLEANFILES+=	elf32-target.h elf64-target.h
elf32-target.h: elfxx-target.h
	sed -e s/NN/32/g ${.ALLSRC} > ${.TARGET}

elf64-target.h: elfxx-target.h
	sed -e s/NN/64/g ${.ALLSRC} > ${.TARGET}

.include <bsd.lib.mk>
