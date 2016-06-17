#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

# These assembly files can't be assembld with -traditional, so we
# need another build rule than the one in the toplevel Makefile.
.S.o:
	$(CC) $(AFLAGS) -c $< -o $*.o

all: kernel.o head.o

O_TARGET := kernel.o

export-objs := crisksyms.o

obj-y   := process.o signal.o entry.o traps.o irq.o \
           ptrace.o setup.o time.o sys_cris.o shadows.o \
	   debugport.o semaphore.o

obj-$(CONFIG_MODULES)    += crisksyms.o
obj-$(CONFIG_ETRAX_KGDB) += kgdb.o
obj-$(CONFIG_ETRAX_FAST_TIMER) += fasttimer.o
obj-$(CONFIG_ETRAX_DEBUG_INTERRUPT) += debug.o

# This dependency isn't caught by mkdep.  See entry.S.
entry.o: entryoffsets.s

# We don't want debug info for the dummy entryoffsets functions, and the
# assembler gets confused when file directives are .if 0:d out.
entryoffsets.s: entryoffsets.c
	$(CC) $(subst -g,,$(CFLAGS)) -S -c $<

clean:

include $(TOPDIR)/Rules.make
