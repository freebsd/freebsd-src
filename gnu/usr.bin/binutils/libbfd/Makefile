# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/bfd ${SRCDIR}/opcodes

LIB=	bfd
SRCS+=	archive.c archive64.c archures.c bfd.c binary.c cache.c \
	coffgen.c corefile.c elf.c elf-eh-frame.c elf-strtab.c format.c \
	hash.c ihex.c init.c libbfd.c linker.c merge.c opncls.c reloc.c \
	section.c srec.c stab-syms.c stabs.c syms.c targets.c tekhex.c \
	targmatch.h dwarf1.c dwarf2.c config.h
.if (${TARGET_ARCH} == "alpha" || ${TARGET_ARCH} == "ia64" || ${TARGET_ARCH} == "sparc64")
WARNS=	2
.endif
CFLAGS+= -I${SRCDIR}/bfd
CFLAGS+= -DBFD_VERSION=\"${VERSION_DATE}\" -DBFD_VERSION_DATE=${VERSION_DATE}
CFLAGS+= -DBFD_VERSION_STRING=\"${VERSION}\"
INTERNALLIB=	true
CLEANFILES+=	config.h targmatch.h

SELARCH=
.if ${TARGET_ARCH} == "sparc64"
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
.if ${SELVEC} == ""
SELVEC+= &${_v}
.else
SELVEC+= ,&${_v}
.endif
.endfor
CFLAGS+= -DSELECT_VECS="${SELVEC}"

targmatch.h: targmatch.sed config.bfd
	sed -f ${.ALLSRC:M*.sed} ${.ALLSRC:M*.bfd} > ${.TARGET}
	
config.h: config.h.fbsd
.if ${TARGET_ARCH} == "i386"
	sed -e 's,!!TRAD_HEADER!!,"hosts/i386bsd.h",g' ${.ALLSRC} > ${.TARGET}
.else
	sed -e 's,!!TRAD_HEADER!!,,g' ${.ALLSRC} > ${.TARGET}
.endif

.include <bsd.lib.mk>
