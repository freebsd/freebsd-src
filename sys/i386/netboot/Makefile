# Makefile for NETBOOT
#
# Options:
#	-DASK_BOOT	- Ask "Boot from Network (Y/N) ?" at startup
#	-DSMALL_ROM	- Compile for 8K ROMS
#	-DROMSIZE	- Size of EPROM - Must be set (even for .COM files)
#	-DRELOC		- Relocation address (usually 0x90000)
#	-DINCLUDE_WD	- Include Western Digital/SMC support
#	-DINCLUDE_NE	- Include NE1000/NE2000 support
#	-DNE_BASE	- Base I/O address for NE1000/NE2000
#	-DWD_DEFAULT_MEM- Default memory location for WD/SMC cards
#
ROMSIZE=16384
RELOCADDR=0x90000
CFLAGS=	-O2 -DNFS -DINCLUDE_WD -DINCLUDE_NE -DROMSIZE=$(ROMSIZE) \
	-DRELOC=$(RELOCADDR) -DNE_BASE=0x320 -DWD_DEFAULT_MEM=0xD0000

HDRS=netboot.h
COBJS=main.o misc.o ether.o bootmenu.o
SSRCS=start2.S
SOBJS=start2.o

.SUFFIXES:	.c .S .s .o

all:	netboot.com netboot.rom

makerom:	makerom.c
	cc -o makerom -DROMSIZE=$(ROMSIZE) makerom.c

netboot.com:	$(COBJS) $(SSRCS)
	cc -c $(CFLAGS) $(SSRCS)
	ld -e _start -T $(RELOCADDR) -N $(SOBJS) $(COBJS)
	strip a.out
	size a.out
	dd ibs=32 skip=1 <a.out >netboot.com

netboot.rom:	$(COBJS) $(SSRCS) makerom
	cc -c $(CFLAGS) -DBOOTROM $(SSRCS)
	ld -e _start -T $(RELOCADDR) -N $(SOBJS) $(COBJS)
	strip a.out
	size a.out
	dd ibs=32 skip=1 <a.out >netboot.rom
	./makerom netboot.rom

test:	netboot.com
	mount -t pcfs /dev/fd0a /msdos
	cp netboot.com /msdos/netboot.com
	cp netboot.rom /msdos/netboot.rom
	umount /msdos
clean:
	rm -f $(COBJS) $(SOBJS) *.s netboot.com netboot.rom a.out makerom

.c.o:	Makefile $(HDRS)
	cc $(CFLAGS) -c $<

.c.s:	Makefile $(HDRS)
	cc $(CFLAGS) -S $<


