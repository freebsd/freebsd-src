#
# $FreeBSD: src/gnu/usr.bin/binutils/libbfd/Makefile,v 1.9.2.1 2000/07/06 22:15:54 obrien Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/bfd ${SRCDIR}/opcodes

LIB=		bfd
SRCS+=		archive.c archures.c bfd.c binary.c cache.c \
		coffgen.c corefile.c elf.c format.c hash.c ihex.c \
		init.c libbfd.c linker.c opncls.c reloc.c section.c \
		srec.c stab-syms.c stabs.c syms.c targets.c tekhex.c \
		targmatch.h dwarf1.c dwarf2.c
CFLAGS+=	-I${SRCDIR}/bfd
CFLAGS+=	-DBFD_VERSION=\"${VERSION}\"
NOPROFILE=	true
NOPIC=		true
INTERNALLIB=	true
INTERNALSTATICLIB=true
CLEANFILES+=	targmatch.h

# If set, BINUTILSDISTDIR is the path to a directory containing the full GNU
# binutils release. FreeBSD only distributes the bits that are required to
# build native architectures. BINUTILSDISTDIR is needed to build cross tools.
.if	defined(BINUTILSDISTDIR) && exists(${BINUTILSDISTDIR})
.PATH:		${BINUTILSDISTDIR}/bfd ${BINUTILSDISTDIR}/opcodes
CFLAGS+=	-I${BINUTILSDISTDIR}/bfd
CFLAGS+=	-I${BINUTILSDISTDIR}/include
.endif

SELARCH=
.if ${BINUTIL_ARCH} == "sparc64"
SELARCH=&bfd_sparc_arch
.else
.for _a in ${ARCHS}
.if ${SELARCH} == ""
SELARCH+=&bfd_${_a}_arch
.else
SELARCH+=,&bfd_${_a}_arch
.endif
.endfor
.endif
CFLAGS+=	-DSELECT_ARCHITECTURES="${SELARCH}"

SELVEC=
.for _v in ${VECS}
.if ${SELVEC} == ""
SELVEC+=&${_v}
.else
SELVEC+=,&${_v}
.endif
.endfor
CFLAGS+=	-DSELECT_VECS="${SELVEC}"

targmatch.h: targmatch.sed config.bfd
	sed -f ${.ALLSRC:M*.sed} ${.ALLSRC:M*.bfd} > ${.TARGET}

.include <bsd.lib.mk>
