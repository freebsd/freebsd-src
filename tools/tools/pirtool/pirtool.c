/*
 * Copyright (c) 2002-2005 Bruce M. Simpson.
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by Bruce M. Simpson.
 * 4. Neither the name of Bruce M. Simpson nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRUCE M. SIMPSON AND AFFILIATES
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/memrange.h>
#include <sys/stat.h>
#include <machine/endian.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "pirtable.h"

#define _PATH_DEVMEM	"/dev/mem"

void usage(void);
void banner(void);
pir_table_t *find_pir_table(unsigned char *base);
void dump_pir_table(pir_table_t *pir, char *map_addr);
void print_irq_line(char line, u_int8_t link, u_int8_t irqs[2]);
char *lookup_southbridge(u_int32_t id);

char *progname = NULL;

int
main(int argc, char *argv[])
{
	int ch, r;
	int err = -1;
	int mem_fd = -1;
	pir_table_t *pir = NULL;
	void *map_addr = MAP_FAILED;
	char *real_pir;

	progname = basename(argv[0]);
	while ((ch = getopt(argc, argv, "h")) != -1)
		switch (ch) {
		case 'h':
		default:
			usage();
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();

	banner();
	/*
	 * Map the PIR region into our process' linear space.
	 */
	if ((mem_fd = open(_PATH_DEVMEM, O_RDONLY)) == -1) {
		perror("open");
		goto cleanup;
	}
	map_addr = mmap(NULL, PIR_SIZE, PROT_READ, MAP_SHARED, mem_fd,
	    PIR_BASE);
	if (map_addr == MAP_FAILED) {
		perror("mmap");
		goto cleanup;
	}
	/*
	 * Find and print the PIR table.
	 */
	if ((pir = find_pir_table(map_addr)) == NULL) {
		fprintf(stderr, "PIR table signature not found.\r\n");
	} else {
		dump_pir_table(pir, map_addr);
		err = 0;
	}

cleanup:
	if (map_addr != MAP_FAILED)
		munmap(map_addr, PIR_SIZE);
	if (mem_fd != -1)
		close(mem_fd);

	exit ((err == 0) ? EXIT_SUCCESS : EXIT_FAILURE);
}

void
usage(void)
{

	fprintf(stderr, "usage: %s [-h]\r\n", progname);
	fprintf(stderr, "-h\tdisplay this message\r\n", progname);
	exit(EXIT_FAILURE);
}

void
banner(void)
{

	fprintf(stderr, "PIRTOOL for FreeBSD (c) 2002 Bruce M. Simpson\r\n");
	fprintf(stderr,
	    "---------------------------------------------\r\n\r\n");
}

pir_table_t *
find_pir_table(unsigned char *base)
{
	unsigned int csum = 0;
	unsigned char *p, *pend;
	pir_table_t *pir = NULL;

	/*
	 * From Microsoft's PCI IRQ Routing Table Specification 1.0:
	 *
	 * The PCI IRQ Routing Table can be detected by searching the
	 * system memory from F0000h to FFFFFh at every 16-byte boundary
	 * for the PCI IRQ routing signature ("$PIR").
	 */
	pend = base + PIR_SIZE;
	for (p = base; p < pend; p += 16) {
		if (strncmp(p, "$PIR", 4) == 0) {
			pir = (pir_table_t *)p;
			break;
		}
	}

	/*
	 * Now validate the table:
	 * Version: Must be 1.0.
	 * Table size: Must be larger than 32 and must be a multiple of 16.
	 * Checksum: The entire structure's checksum must be 0.
	 */
	if (pir && (pir->major == 1) && (pir->minor == 0) &&
	    (pir->size > 32) && ((pir->size % 16) == 0)) {
		p = (unsigned char *)pir;
		pend = p + pir->size;

		while (p < pend)
			csum += *p++;

		if ((csum % 256) != 0)
			fprintf(stderr,
			    "WARNING: PIR table checksum is invalid.\n");
	}

	return ((pir_table_t *)pir);
}

void
dump_pir_table(pir_table_t *pir, char *map_addr)
{
	int i, num_slots;
	pir_entry_t *p, *pend;

	num_slots = (pir->size - offsetof(pir_table_t, entry[0])) / 16;

	printf( "PCI Interrupt Routing Table at 0x%08lX\r\n"
	    "-----------------------------------------\r\n"
	    "0x%02x: Signature:          %c%c%c%c\r\n"
	    "0x%02x: Version:            %u.%u\r\n"
	    "0x%02x: Size:               %u bytes (%u entries)\r\n"
	    "0x%02x: Device:             %u:%u:%u\r\n"
	    "0x%02x: PCI Exclusive IRQs: 0x%08lX\r\n"
	    "0x%02x: Compatible with:    0x%08X %s\r\n"
	    "0x%02x: Miniport Data:      0x%08X\r\n"
	    "0x%02x: Checksum:           0x%02X\r\n"
	    "\r\n",
	    (u_int32_t)(((char *)pir - map_addr) + PIR_BASE),
	    offsetof(pir_table_t, signature),
	    ((char *)&pir->signature)[0],
	    ((char *)&pir->signature)[1],
	    ((char *)&pir->signature)[2],
	    ((char *)&pir->signature)[3],
	    offsetof(pir_table_t, minor),
	    pir->major, pir->minor,
	    offsetof(pir_table_t, size),
	    pir->size,
	    num_slots,
	    offsetof(pir_table_t, bus),
	    pir->bus,
	    PIR_DEV(pir->devfunc),
	    PIR_FUNC(pir->devfunc),
	    offsetof(pir_table_t, excl_irqs),
	    pir->excl_irqs,
	    offsetof(pir_table_t, compatible),
	    pir->compatible,
	    lookup_southbridge(pir->compatible),
	    offsetof(pir_table_t, miniport_data),
	    pir->miniport_data,
	    offsetof(pir_table_t, checksum),
	    pir->checksum);

	p = pend = &pir->entry[0];
	pend += num_slots;
	for (i = 0; p < pend; i++, p++) {
		printf("Entry %u: Device %u:%u:%u Slot %u%s\r\n", i, p->bus,
		    PIR_DEV(p->devfunc), PIR_FUNC(p->devfunc),
		    p->slot, p->slot == 0 ? " (on-board)" : "");
		print_irq_line('A', p->inta_link, p->inta_irqs);
		print_irq_line('B', p->intb_link, p->intb_irqs);
		print_irq_line('C', p->intc_link, p->intc_irqs);
		print_irq_line('D', p->intd_link, p->intd_irqs);
	}
}

/*
 * Print interrupt map for a given PCI interrupt line.
 */
void
print_irq_line(char line, u_int8_t link, u_int8_t irqs[2])
{
	u_int16_t map;
	int i;

	printf("\tINT%c: %02xh ", line, link);
	if (link == 0) {
		printf("(not connected)\r\n");
		return;
	}

	map = irqs[1];
	map <<= 8;
	map |= irqs[0];
	if (map == 0) {
		printf("(not routable)\r\n");
		return;
	}

	printf("routable irqs:");
	for (i = 0; i < 16; i++) {
		if (map & 0x1)
			printf(" %d", i);
		map >>= 1;
	}

	printf("\r\n");
}

/*
 * Lookup textual descriptions for commonly-used south-bridges.
 */
char *
lookup_southbridge(u_int32_t id)
{

	switch (id) {
	case 0x06861106:
		return ("VIA VT82C686/686A/686B");
	case 0x122E8086:
		return ("Intel 82371FB (Triton I)");
	case 0x70008086:
		return ("Intel 82371SB (Triton II/PIIX3)");
	default:
		return ("unknown chipset");
	}
}
