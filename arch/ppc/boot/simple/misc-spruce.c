/*
 * arch/ppc/boot/spruce/misc.c
 *
 * Misc. bootloader code for IBM Spruce reference platform
 *
 * Authors: Johnnie Peters <jpeters@mvista.com>
 *	    Matt Porter <mporter@mvista.com>
 *
 * Derived from arch/ppc/boot/prep/misc.c
 *
 * 2000-2001 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#include <linux/types.h>
#include <linux/elf.h>
#include <linux/config.h>
#include <linux/pci.h>

#include <asm/page.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/bootinfo.h>

#include "zlib.h"

#ifdef CONFIG_CMDLINE
#define CMDLINE CONFIG_CMDLINE
#else
#define CMDLINE ""
#endif

#if defined(CONFIG_SERIAL_CONSOLE) || defined(CONFIG_VGA_CONSOLE)
#define INTERACTIVE_CONSOLE	1
#endif

/* Define some important locations of the Spruce. */
#define SPRUCE_PCI_CONFIG_ADDR	0xfec00000
#define SPRUCE_PCI_CONFIG_DATA	0xfec00004
#define SPRUCE_ISA_IO_BASE	0xf8000000

unsigned long com_port;

char *avail_ram;
char *end_avail;

/* The linker tells us where the image is. */
extern char __image_begin, __image_end;
extern char __ramdisk_begin, __ramdisk_end;
extern char _end[];

char cmd_preset[] = CMDLINE;
char cmd_buf[256];
char *cmd_line = cmd_buf;

unsigned long initrd_size = 0;

char *zimage_start;
int zimage_size;

extern void udelay(long);
extern void puts(const char *);
extern void putc(const char c);
extern void puthex(unsigned long val);
extern int getc(void);
extern int tstc(void);
extern void gunzip(void *, int, unsigned char *, int *);

extern unsigned long serial_init(int chan, void *ignored);

/* PCI configuration space access routines. */
unsigned int *pci_config_address = (unsigned int *)SPRUCE_PCI_CONFIG_ADDR;
unsigned char *pci_config_data   = (unsigned char *)SPRUCE_PCI_CONFIG_DATA;

void cpc700_pcibios_read_config_byte(unsigned char bus, unsigned char dev_fn,
			     unsigned char offset, unsigned char *val)
{
	out_le32(pci_config_address,
		 (((bus & 0xff)<<16) | (dev_fn<<8) | (offset&0xfc) | 0x80000000));

	*val= (in_le32((unsigned *)pci_config_data) >> (8 * (offset & 3))) & 0xff;
}

void cpc700_pcibios_write_config_byte(unsigned char bus, unsigned char dev_fn,
			     unsigned char offset, unsigned char val)
{
	out_le32(pci_config_address,
		 (((bus & 0xff)<<16) | (dev_fn<<8) | (offset&0xfc) | 0x80000000));

	out_8(pci_config_data + (offset&3), val);
}

void cpc700_pcibios_read_config_word(unsigned char bus, unsigned char dev_fn,
			     unsigned char offset, unsigned short *val)
{
	out_le32(pci_config_address,
		 (((bus & 0xff)<<16) | (dev_fn<<8) | (offset&0xfc) | 0x80000000));

	*val= in_le16((unsigned short *)(pci_config_data + (offset&3)));
}

void cpc700_pcibios_write_config_word(unsigned char bus, unsigned char dev_fn,
			     unsigned char offset, unsigned short val)
{
	out_le32(pci_config_address,
		 (((bus & 0xff)<<16) | (dev_fn<<8) | (offset&0xfc) | 0x80000000));

	out_le16((unsigned short *)(pci_config_data + (offset&3)), val);
}

void cpc700_pcibios_read_config_dword(unsigned char bus, unsigned char dev_fn,
			     unsigned char offset, unsigned int *val)
{
	out_le32(pci_config_address,
		 (((bus & 0xff)<<16) | (dev_fn<<8) | (offset&0xfc) | 0x80000000));

	*val= in_le32((unsigned *)pci_config_data);
}

void cpc700_pcibios_write_config_dword(unsigned char bus, unsigned char dev_fn,
			     unsigned char offset, unsigned int val)
{
	out_le32(pci_config_address,
		 (((bus & 0xff)<<16) | (dev_fn<<8) | (offset&0xfc) | 0x80000000));

	out_le32((unsigned *)pci_config_data, val);
}

unsigned long isa_io_base = SPRUCE_ISA_IO_BASE;

#define PCNET32_WIO_RDP		0x10
#define PCNET32_WIO_RAP		0x12
#define PCNET32_WIO_RESET	0x14

#define PCNET32_DWIO_RDP	0x10
#define PCNET32_DWIO_RAP	0x14
#define PCNET32_DWIO_RESET	0x18

/* Processor interface config register access */
#define PIFCFGADDR 0xff500000
#define PIFCFGDATA 0xff500004

#define PLBMIFOPT 0x18 /* PLB Master Interface Options */

#define MEM_MBEN	0x24
#define MEM_TYPE	0x28
#define MEM_B1SA	0x3c
#define MEM_B1EA	0x5c
#define MEM_B2SA	0x40
#define MEM_B2EA	0x60

unsigned long
load_kernel(unsigned long load_addr, int num_words, unsigned long cksum)
{
#ifdef INTERACTIVE_CONSOLE
	int timer = 0;
	char ch;
#endif
	char *cp;
	int loop;
	int csr0;
	int csr_id;
	volatile int *mem_addr = (int *)0xff500008;
	volatile int *mem_data = (int *)0xff50000c;
	int mem_size = 0;
	unsigned long mem_mben;
	unsigned long mem_type;
	unsigned long mem_start;
	unsigned long mem_end;
	volatile int *pif_addr = (int *)0xff500000;
	volatile int *pif_data = (int *)0xff500004;
	int pci_devfn;
	int found_multi = 0;
	unsigned short vendor;
	unsigned short device;
	unsigned short command;
	unsigned char header_type;
	unsigned int bar0;

	/* Initialize the serial console port */
	com_port = serial_init(0, NULL);

	/*
	 * Gah, these firmware guys need to learn that hardware
	 * byte swapping is evil! Disable all hardware byte
	 * swapping so it doesn't hurt anyone.
	 */
	*pif_addr = PLBMIFOPT;
	asm("sync");
	*pif_data = 0x00000000;
	asm("sync");

	/* Get the size of memory from the memory controller. */
	*mem_addr = MEM_MBEN;
	asm("sync");
	mem_mben = *mem_data;
	asm("sync");
	for(loop = 0; loop < 1000; loop++);

	*mem_addr = MEM_TYPE;
	asm("sync");
	mem_type = *mem_data;
	asm("sync");
	for(loop = 0; loop < 1000; loop++);

	*mem_addr = MEM_TYPE;
	/* Confirm bank 1 has DRAM memory */
	if ((mem_mben & 0x40000000) &&
				((mem_type & 0x30000000) == 0x10000000)) {
		*mem_addr = MEM_B1SA;
		asm("sync");
		mem_start = *mem_data;
		asm("sync");
		for(loop = 0; loop < 1000; loop++);

		*mem_addr = MEM_B1EA;
		asm("sync");
		mem_end = *mem_data;
		asm("sync");
		for(loop = 0; loop < 1000; loop++);

		mem_size = mem_end - mem_start + 0x100000;
	}

	/* Confirm bank 2 has DRAM memory */
	if ((mem_mben & 0x20000000) &&
				((mem_type & 0xc000000) == 0x4000000)) {
		*mem_addr = MEM_B2SA;
		asm("sync");
		mem_start = *mem_data;
		asm("sync");
		for(loop = 0; loop < 1000; loop++);

		*mem_addr = MEM_B2EA;
		asm("sync");
		mem_end = *mem_data;
		asm("sync");
		for(loop = 0; loop < 1000; loop++);

		mem_size += mem_end - mem_start + 0x100000;
	}

	/* Search out and turn off the PcNet ethernet boot device. */
	for (pci_devfn = 1; pci_devfn < 0xff; pci_devfn++) {
		if (PCI_FUNC(pci_devfn) && !found_multi)
			continue;

		cpc700_pcibios_read_config_byte(0, pci_devfn,
				PCI_HEADER_TYPE, &header_type);

		if (!PCI_FUNC(pci_devfn))
			found_multi = header_type & 0x80;

		cpc700_pcibios_read_config_word(0, pci_devfn, PCI_VENDOR_ID,
				&vendor);

		if (vendor != 0xffff) {
			cpc700_pcibios_read_config_word(0, pci_devfn,
						PCI_DEVICE_ID, &device);

			/* If this PCI device is the Lance PCNet board then turn it off */
			if ((vendor == PCI_VENDOR_ID_AMD) &&
					(device == PCI_DEVICE_ID_AMD_LANCE)) {

				/* Turn on I/O Space on the board. */
				cpc700_pcibios_read_config_word(0, pci_devfn,
						PCI_COMMAND, &command);
				command |= 0x1;
				cpc700_pcibios_write_config_word(0, pci_devfn,
						PCI_COMMAND, command);

				/* Get the I/O space address */
				cpc700_pcibios_read_config_dword(0, pci_devfn,
						PCI_BASE_ADDRESS_0, &bar0);
				bar0 &= 0xfffffffe;

				/* Reset the PCNet Board */
				inl (bar0+PCNET32_DWIO_RESET);
				inw (bar0+PCNET32_WIO_RESET);

				/* First do a work oriented read of csr0.  If the value is
				 * 4 then this is the correct mode to access the board.
				 * If not try a double word ortiented read.
				 */
				outw(0, bar0 + PCNET32_WIO_RAP);
				csr0 = inw(bar0 + PCNET32_WIO_RDP);

				if (csr0 == 4) {
					/* Check the Chip id register */
					outw(88, bar0 + PCNET32_WIO_RAP);
					csr_id = inw(bar0 + PCNET32_WIO_RDP);

					if (csr_id) {
						/* This is the valid mode - set the stop bit */
						outw(0, bar0 + PCNET32_WIO_RAP);
						outw(csr0, bar0 + PCNET32_WIO_RDP);
					}
				} else {
					outl(0, bar0 + PCNET32_DWIO_RAP);
					csr0 = inl(bar0 + PCNET32_DWIO_RDP);
					if (csr0 == 4) {
						/* Check the Chip id register */
						outl(88, bar0 + PCNET32_WIO_RAP);
						csr_id = inl(bar0 + PCNET32_WIO_RDP);

						if (csr_id) {
							/* This is the valid mode  - set the stop bit*/
							outl(0, bar0 + PCNET32_WIO_RAP);
							outl(csr0, bar0 + PCNET32_WIO_RDP);
						}
					}
				}
			}
		}
	}

	/* assume the chunk below 8M is free */
	end_avail = (char *)0x00800000;

	/*
	 * We link ourself to 0x00800000.  When we run, we relocate
	 * ourselves there.  So we just need __image_begin for the
	 * start. -- Tom
	 */
	zimage_start = (char *)(unsigned long)(&__image_begin);
	zimage_size = (unsigned long)(&__image_end) -
			(unsigned long)(&__image_begin);

	initrd_size = (unsigned long)(&__ramdisk_end) -
		(unsigned long)(&__ramdisk_begin);

	/*
	 * The zImage and initrd will be between start and _end, so they've
	 * already been moved once.  We're good to go now. -- Tom
	 */
	avail_ram = (char *)PAGE_ALIGN((unsigned long)_end);
	puts("zimage at:     "); puthex((unsigned long)zimage_start);
	puts(" "); puthex((unsigned long)(zimage_size+zimage_start));
	puts("\n");

	if ( initrd_size ) {
		puts("initrd at:     ");
		puthex((unsigned long)(&__ramdisk_begin));
		puts(" "); puthex((unsigned long)(&__ramdisk_end));puts("\n");
	}

	avail_ram = (char *)0x00400000;
	end_avail = (char *)0x00800000;
	puts("avail ram:     "); puthex((unsigned long)avail_ram); puts(" ");
	puthex((unsigned long)end_avail); puts("\n");

	/* Display standard Linux/PPC boot prompt for kernel args */
	puts("\nLinux/PPC load: ");
	cp = cmd_line;
	memcpy (cmd_line, cmd_preset, sizeof(cmd_preset));
	while ( *cp )
		putc(*cp++);
#ifdef INTERACTIVE_CONSOLE
	/*
	 * If they have a console, allow them to edit the command line.
	 * Otherwise, don't bother wasting the five seconds.
	 */
	while (timer++ < 5*1000) {
		if (tstc()) {
			while ((ch = getc()) != '\n' && ch != '\r') {
				if (ch == '\b') {
					if (cp != cmd_line) {
						cp--;
						puts("\b \b");
					}
				} else {
					*cp++ = ch;
					putc(ch);
				}
			}
			break;  /* Exit 'timer' loop */
		}
		udelay(1000);  /* 1 msec */
	}
#endif
	*cp = 0;
	puts("\n");

	puts("Uncompressing Linux...");

	gunzip(0, 0x400000, zimage_start, &zimage_size);

	puts("done.\n");

	{
		struct bi_record *rec;
		unsigned long initrd_loc;
		unsigned long rec_loc = _ALIGN((unsigned long)(zimage_size) +
				(1 << 20) - 1, (1 << 20));
		rec = (struct bi_record *)rec_loc;

		/* We need to make sure that the initrd and bi_recs do not
		 * overlap. */
		if ( initrd_size ) {
			initrd_loc = (unsigned long)(&__ramdisk_begin);
			/* If the bi_recs are in the middle of the current
			 * initrd, move the initrd to the next MB
			 * boundary. */
			if ((rec_loc > initrd_loc) &&
					((initrd_loc + initrd_size)
					 > rec_loc)) {
				initrd_loc = _ALIGN((unsigned long)(zimage_size)
						+ (2 << 20) - 1, (2 << 20));
			 	memmove((void *)initrd_loc, &__ramdisk_begin,
					 initrd_size);
		         	puts("initrd moved:  "); puthex(initrd_loc);
			 	puts(" "); puthex(initrd_loc + initrd_size);
			 	puts("\n");
			}
		}

		rec->tag = BI_FIRST;
        	rec->size = sizeof(struct bi_record);
        	rec = (struct bi_record *)((unsigned long)rec + rec->size);

        	rec->tag = BI_BOOTLOADER_ID;
        	memcpy( (void *)rec->data, "spruceboot", 11);
        	rec->size = sizeof(struct bi_record) + 10 + 1;
        	rec = (struct bi_record *)((unsigned long)rec + rec->size);

        	rec->tag = BI_MEMSIZE;
        	rec->data[0] = mem_size;
        	rec->size = sizeof(struct bi_record) + sizeof(unsigned long);
        	rec = (struct bi_record *)((unsigned long)rec + rec->size);

        	rec->tag = BI_CMD_LINE;
        	memcpy( (char *)rec->data, cmd_line, strlen(cmd_line)+1);
        	rec->size = sizeof(struct bi_record) + strlen(cmd_line) + 1;
        	rec = (struct bi_record *)((ulong)rec + rec->size);

		if ( initrd_size ) {
			rec->tag = BI_INITRD;
			rec->data[0] = initrd_loc;
			rec->data[1] = initrd_size;
			rec->size = sizeof(struct bi_record) + 2 *
				sizeof(unsigned long);
			rec = (struct bi_record *)((unsigned long)rec +
					rec->size);
		}

        	rec->tag = BI_LAST;
        	rec->size = sizeof(struct bi_record);
        	rec = (struct bi_record *)((unsigned long)rec + rec->size);
	}

	puts("Now booting the kernel\n");

	return 0;
}
