#
# $Id: Makefile,v 1.3 1998/03/12 05:14:09 jdp Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/bfd

LIB=		bfd
SRCS+=		archive.c archures.c bfd.c binary.c cache.c \
		coffgen.c corefile.c elf.c format.c hash.c ihex.c \
		init.c libbfd.c linker.c opncls.c reloc.c section.c \
		srec.c stab-syms.c stabs.c syms.c targets.c tekhex.c
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

beforedepend: targmatch.h

targets.o: targmatch.h

targmatch.h: targmatch.sed config.bfd
	sed -f ${.ALLSRC:M*.sed} ${.ALLSRC:M*.bfd} > ${.TARGET}

.include <bsd.lib.mk>
