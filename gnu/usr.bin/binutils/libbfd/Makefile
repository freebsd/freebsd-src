#
# $Id$
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/bfd

LIB=		bfd
SRCS=		aout32.c archive.c archures.c bfd.c binary.c \
		cache.c coff-i386.c coffgen.c cofflink.c corefile.c \
		cpu-i386.c elf.c elf32-i386.c elf32.c elflink.c \
		format.c hash.c i386bsd.c i386freebsd.c ihex.c \
		init.c libbfd.c linker.c opncls.c reloc.c section.c \
		srec.c stab-syms.c stabs.c syms.c targets.c tekhex.c
CFLAGS+=	-DDEFAULT_VECTOR=bfd_elf32_i386_vec
CFLAGS+=	-DSELECT_VECS="&bfd_elf32_i386_vec,&i386freebsd_vec,&i386bsd_vec,&i386coff_vec"
CFLAGS+=	-DSELECT_ARCHITECTURES="&bfd_i386_arch"
CFLAGS+=	-DHAVE_bfd_elf32_i386_vec
CFLAGS+=	-DHAVE_i386freebsd_vec
CFLAGS+=	-DHAVE_i386bsd_vec
CFLAGS+=	-DHAVE_i386coff_vec
CFLAGS+=	-I${SRCDIR}/bfd
NOPROFILE=	true
NOPIC=		true
CLEANFILES+=	elf32-target.h targmatch.h

beforedepend: elf32-target.h targmatch.h

targets.o: targmatch.h

elf32-i386.o: elf32-target.h

elf32-target.h: elfxx-target.h
	sed -e s/NN/32/g ${.ALLSRC} > ${.TARGET}

targmatch.h: targmatch.sed config.bfd
	sed -f ${.ALLSRC:M*.sed} ${.ALLSRC:M*.bfd} > ${.TARGET}

install:

.include <bsd.lib.mk>
