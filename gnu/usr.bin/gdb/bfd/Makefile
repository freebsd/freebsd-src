LIB = bfd

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/bfd

SRCS =	libbfd.c opncls.c bfd.c archive.c targets.c cache.c \
	 archures.c coff-i386.c aout32.c \
	 srec.c \
	 ecoff.c ecofflink.c \
	 coffgen.c format.c \
	 section.c syms.c stab-syms.c reloc.c init.c \
	 trad-core.c \
	 i386aout.c \
	 i386freebsd.c i386bsd.c \
	 cpu-i386.c \
	 elf.c \
	 hash.c linker.c corefile.c binary.c \
	 tekhex.c ihex.c

CFLAGS+= -I${.CURDIR}/. -I${.CURDIR}/../gdb/.
CFLAGS+= -DDEFAULT_VECTOR=i386freebsd_vec \
	 -DSELECT_VECS='&i386freebsd_vec,&i386bsd_vec' \
         -DSELECT_ARCHITECTURES='&bfd_i386_arch' -DTRAD_CORE

NOPROFILE=no
NOPIC=no

install:
	@echo -n

.include <bsd.lib.mk>
