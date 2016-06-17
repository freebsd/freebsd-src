#
# Makefile for the DECstation family specific parts of the kernel
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

netboot: all
	mipsel-linux-ld -N -G 0 -T ld.ecoff ../../boot/zImage \
		dec_boot.o ramdisk.img -o nbImage

all: dec_boot.o

O_TARGET := dec_boot.o

obj-y	 := decstation.o

clean:
	rm -f nbImage

include $(TOPDIR)/Rules.make
