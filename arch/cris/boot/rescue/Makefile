#
# Makefile for rescue code
#
ifndef TOPDIR
TOPDIR = ../../../..
HPATH = $(TOPDIR)/include
export HPATH
endif
CC = gcc-cris -mlinux -I $(TOPDIR)/include
CFLAGS = -O2
LD = gcc-cris -mlinux -nostdlib
OBJCOPY = objcopy-cris

all: rescue.bin testrescue.bin kimagerescue.bin

rescue: rescue.bin
	# do nothing

rescue.bin: head.o
	$(LD) -T rescue.ld -o rescue.o head.o
	$(OBJCOPY) -O binary --remove-section=.bss rescue.o rescue.bin
	cp rescue.bin $(TOPDIR)

testrescue.bin: testrescue.o
	$(OBJCOPY) -O binary --remove-section=.bss testrescue.o tr.bin
# Pad it to 784 bytes
	dd if=/dev/zero of=tmp2423 bs=1 count=784
	cat tr.bin tmp2423 >testrescue_tmp.bin
	dd if=testrescue_tmp.bin of=testrescue.bin bs=1 count=784
	rm tr.bin tmp2423 testrescue_tmp.bin

kimagerescue.bin: kimagerescue.o
	$(OBJCOPY) -O binary --remove-section=.bss kimagerescue.o ktr.bin
# Pad it to 784 bytes, that's what the rescue loader expects
	dd if=/dev/zero of=tmp2423 bs=1 count=784
	cat ktr.bin tmp2423 >kimagerescue_tmp.bin
	dd if=kimagerescue_tmp.bin of=kimagerescue.bin bs=1 count=784
	rm ktr.bin tmp2423 kimagerescue_tmp.bin

head.o: head.S 
	$(CC) -D__ASSEMBLY__ -traditional -c $< -o $*.o

testrescue.o: testrescue.S
	$(CC) -D__ASSEMBLY__ -traditional -c $< -o $*.o

kimagerescue.o: kimagerescue.S
	$(CC) -D__ASSEMBLY__ -traditional -c $< -o $*.o

clean:
	rm -f *.o *.bin

fastdep:

modules:

modules_install:

depend:
	$(CC) -M *.S > .depend

-include .depend
