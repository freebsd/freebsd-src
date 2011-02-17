/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "at91rm9200.h"
#include "lib.h"
#include "at91rm9200_lowlevel.h"
#include "spi_flash.h"

#define LOADER_OFFSET 0
#define FPGA_OFFSET  (15 * FLASH_PAGE_SIZE)
#define OFFSET FPGA_OFFSET

int
main(void)
{
	int len, i, j, off, sec;
	char *addr = (char *)SDRAM_BASE + (1 << 20); /* download at + 1MB */
	char *addr2 = (char *)SDRAM_BASE + (2 << 20); /* readback to + 2MB */
	char *addr3 = (char *)SDRAM_BASE + (3 << 20); /* extra copy at + 3MB */

	SPI_InitFlash();
	printf("Waiting for data\n");
	while ((len = xmodem_rx(addr)) == -1)
		continue;
	// Need extra copy at addr3
	memcpy(addr3, addr, (len + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE);
	printf("Writing %u bytes to flash at %u\n", len, OFFSET);
	for (i = 0; i < len; i+= FLASH_PAGE_SIZE) {
		for (j = 0; j < 10; j++) {
			off = i + OFFSET;
			SPI_WriteFlash(off, addr + i, FLASH_PAGE_SIZE);
			SPI_ReadFlash(off, addr2 + i, FLASH_PAGE_SIZE);
			if (p_memcmp(addr3 + i, addr2 + i, FLASH_PAGE_SIZE) == 0)
				break;
		}
		if (j >= 10)
			printf("Bad Readback at %u\n", i);
	}
	sec = GetSeconds() + 2;
	while (sec <= GetSeconds())
	    continue;
	printf("Done\n");
	reset();
	return (1);
}
