# $Id: Makefile,v 1.69 2001/11/19 04:09:53 davem Exp $
# Makefile for the linux kernel.
#

SH = $(CONFIG_SHELL)

USE_STANDARD_AS_RULE := true
EXTRA_AFLAGS := -ansi

all: kernel.o head.o init_task.o

O_TARGET := kernel.o

export-objs	:= sparc64_ksyms.o
obj-y		:= process.o setup.o cpu.o idprom.o \
		   traps.o devices.o auxio.o \
		   irq.o ptrace.o time.o sys_sparc.o signal.o \
		   unaligned.o central.o pci.o starfire.o semaphore.o \
		   power.o sbus.o iommu_common.o sparc64_ksyms.o chmc.o

obj-$(CONFIG_PCI)	 += ebus.o isa.o pci_common.o pci_iommu.o \
			    pci_psycho.o pci_sabre.o pci_schizo.o
obj-$(CONFIG_SMP)	 += smp.o trampoline.o
obj-$(CONFIG_SPARC32_COMPAT) += sys32.o sys_sparc32.o signal32.o ioctl32.o
obj-$(CONFIG_BINFMT_ELF32) += binfmt_elf32.o
obj-$(CONFIG_BINFMT_AOUT32) += binfmt_aout32.o

ifdef CONFIG_SUNOS_EMUL
  obj-y += sys_sunos32.o sunos_ioctl32.o
else
  ifdef CONFIG_SOLARIS_EMUL
    obj-y += sys_sunos32.o sunos_ioctl32.o
  endif
endif

head.o: head.S ttable.S itlb_base.S dtlb_base.S dtlb_backend.S dtlb_prot.S \
	etrap.S rtrap.S winfixup.S entry.S
	$(CC) $(AFLAGS) -ansi -c $*.S -o $*.o

#
# This is just to get the dependencies...
#
binfmt_elf32.o: $(TOPDIR)/fs/binfmt_elf.c

ifneq ($(NEW_GCC),y)
  CMODEL_CFLAG := -mmedlow
else
  CMODEL_CFLAG := -m64 -mcmodel=medlow
endif

check_asm: dummy
	@if [ ! -r $(HPATH)/asm/asm_offsets.h ] ; then \
	  touch $(HPATH)/asm/asm_offsets.h ; \
	fi
	@echo "/* Automatically generated. Do not edit. */" > asm_offsets.h
	@echo "#ifndef __ASM_OFFSETS_H__" >> asm_offsets.h
	@echo "#define __ASM_OFFSETS_H__" >> asm_offsets.h
	@echo >> asm_offsets.h
	@echo "#include <linux/config.h>" >> asm_offsets.h
	@echo >> asm_offsets.h
	@echo "#include <linux/config.h>" > tmp.c
	@echo "#include <linux/sched.h>" >> tmp.c
	$(CPP) $(CPPFLAGS) -P tmp.c -o tmp.i
	@echo "/* Automatically generated. Do not edit. */" > check_asm_data.c
	@echo "#include <linux/config.h>" >> check_asm_data.c
	@echo "#include <linux/sched.h>" >> check_asm_data.c
	@echo 'unsigned int check_asm_data[] = {' >> check_asm_data.c
	$(SH) ./check_asm.sh -data task tmp.i check_asm_data.c
	$(SH) ./check_asm.sh -data mm tmp.i check_asm_data.c
	$(SH) ./check_asm.sh -data thread tmp.i check_asm_data.c
	@echo '};' >> check_asm_data.c
	$(CC) $(CPPFLAGS) $(CMODEL_CFLAG) -ffixed-g4 -S -o check_asm_data.s check_asm_data.c
	@echo "/* Automatically generated. Do not edit. */" > check_asm.c
	@echo 'extern int printf(const char *fmt, ...);' >>check_asm.c
	@echo 'unsigned int check_asm_data[] = {' >> check_asm.c
	$(SH) ./check_asm.sh -ints check_asm_data.s check_asm.c
	@echo '};' >> check_asm.c
	@echo 'int main(void) {' >> check_asm.c
	@echo 'int i = 0;' >> check_asm.c
	$(SH) ./check_asm.sh -printf task tmp.i check_asm.c
	$(SH) ./check_asm.sh -printf mm tmp.i check_asm.c
	$(SH) ./check_asm.sh -printf thread tmp.i check_asm.c
	@echo 'return 0; }' >> check_asm.c
	@rm -f tmp.[ci] check_asm_data.[cs]
	$(HOSTCC) -o check_asm check_asm.c
	./check_asm >> asm_offsets.h
	@rm -f check_asm check_asm.c
	@echo >> asm_offsets.h
	@echo "#endif /* __ASM_OFFSETS_H__ */" >> asm_offsets.h
	@if test -r $(HPATH)/asm/asm_offsets.h; then \
	  if cmp -s asm_offsets.h $(HPATH)/asm/asm_offsets.h; then \
	    echo $(HPATH)/asm/asm_offsets.h is unchanged; \
	    rm -f asm_offsets.h; \
	  else \
	    mv -f asm_offsets.h $(HPATH)/asm/asm_offsets.h; \
	  fi; \
	else \
	  mv -f asm_offsets.h $(HPATH)/asm/asm_offsets.h; \
	fi

include $(TOPDIR)/Rules.make
