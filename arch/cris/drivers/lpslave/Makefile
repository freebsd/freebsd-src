#
# Makefile for parallel port slave drivers
#

O_TARGET := lpslavedrivers.o

obj-y = e100lpslavenet.o e100lpslave_code.o


include $(TOPDIR)/Rules.make

e100lpslave_code.o:	e100lpslave.o e100lpslaveld 
	$(CROSS_COMPILE)ld -qmagic -Te100lpslaveld e100lpslave.o -o e100lpslave
	$(CROSS_COMPILE)objcopy -O binary --remove-section=.data --remove-section=.bss e100lpslave e100lpslave.text
	$(CROSS_COMPILE)objcopy -O binary --remove-section=.text --remove-section=.bss e100lpslave e100lpslave.data
	cat e100lpslave.text e100lpslave.data |\
	./bintocarr.pl e100lpslaveprog |\
	$(CC) $(CFLAGS) -pipe -o e100lpslave_code.o -c -x c -
	ls -l e100lpslave.text e100lpslave.data
	rm -f e100lpslave e100lpslave.text e100lpslave.data
