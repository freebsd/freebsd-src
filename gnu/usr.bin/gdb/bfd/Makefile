LIB = bfd

SRCS = libbfd.c opncls.c bfd.c archive.c targets.c cache.c \
	 archures.c coff-i386.c aout32.c \
	 srec.c \
	 ecoff.c ecofflink.c \
	 coffgen.c format.c \
	 section.c core.c syms.c stab-syms.c reloc.c init.c ctor.c \
	 trad-core.c \
	 i386aout.c \
	 freebsd386.c \
	 cpu-i386.c \
	 elf.c elf32.c elf32-i386.c \
	 hash.c linker.c

CFLAGS+= -I$(.CURDIR)/. -I$(.CURDIR)/../gdb/.
CFLAGS+= -DDEFAULT_VECTOR=freebsd386_vec -DSELECT_VECS='&freebsd386_vec' \
         -DSELECT_ARCHITECTURES='bfd_i386_arch' -DTRAD_CORE

NOPROFILE=no
NOPIC=no

install:
	@echo -n

.include <bsd.lib.mk>
