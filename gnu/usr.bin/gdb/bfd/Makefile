LIB = bfd
SRCS = archive.c archures.c bfd.c cache.c coffgen.c core.c ctor.c \
       format.c init.c libbfd.c opncls.c reloc.c seclet.c section.c syms.c \
       targets.c ecoff.c elf.c srec.c freebsd386.c aout32.c stab-syms.c \
       cpu-i386.c trad-core.c

CFLAGS+= -I$(.CURDIR)/. -I$(.CURDIR)/../gdb/.
CFLAGS+= -DDEFAULT_VECTOR=freebsd386_vec -DSELECT_VECS='&freebsd386_vec' \
         -DSELECT_ARCHITECTURES='bfd_i386_arch' -DTRAD_CORE

NOPROFILE=no
NOPIC=no

install:
	@echo -n

.include <bsd.lib.mk>
