#
# $Id: Makefile,v 1.4 1998/05/04 21:16:46 jb Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/bfd

LIB=		bfd
SRCS+=		archive.c archures.c bfd.c binary.c cache.c \
		coffgen.c corefile.c elf.c format.c hash.c ihex.c \
		init.c libbfd.c linker.c opncls.c reloc.c section.c \
		srec.c stab-syms.c stabs.c syms.c targets.c tekhex.c \
		targmatch.h
CFLAGS+=	-I${SRCDIR}/bfd
NOPROFILE=	true
NOPIC=		true
INTERNALLIB=	true
INTERNALSTATICLIB=true
CLEANFILES+=	targmatch.h

SELARCH=
.for _a in ${ARCHS}
.if ${SELARCH} == ""
SELARCH+=&bfd_${_a}_arch
.else
SELARCH+=,&bfd_${_a}_arch
.endif
.endfor
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
