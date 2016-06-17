# File: drivers/scsi/sym53c8xx_2/Makefile
# Makefile for the NCR/SYMBIOS/LSI 53C8XX PCI SCSI controllers driver.

list-multi := sym53c8xx_2.o
sym53c8xx_2-objs := sym_fw.o sym_glue.o sym_hipd.o sym_malloc.o sym_misc.o sym_nvram.o
obj-$(CONFIG_SCSI_SYM53C8XX_2) := sym53c8xx_2.o

EXTRA_CFLAGS += -I.

sym53c8xx_2.o: $(sym53c8xx_2-objs)
	$(LD) -r -o $@ $(sym53c8xx_2-objs)

include $(TOPDIR)/Rules.make

clean:
	rm -f *.o
