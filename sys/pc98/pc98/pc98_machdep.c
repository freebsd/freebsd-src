/*
 * Copyright (c) KATO Takenori, 1996.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>

#include <pc98/pc98/pc98_device.h>

extern int Maxmem;
extern int Maxmem_under16M;

void init_cpu_accel_mem __P((void));
void init_pc98_dmac __P((void));

#ifdef EPSON_MEMWIN
void init_epson_memwin __P((void));

void init_epson_memwin(void)
{
	if (pc98_machine_type & M_EPSON_PC98) {
		if (Maxmem > 3840) {
			if (Maxmem == Maxmem_under16M) {
				Maxmem = 3840;
				Maxmem_under16M = 3840;
			} else if (Maxmem_under16M > 3840) {
				Maxmem_under16M = 3840;
			}
		}

		/* Disable 15MB-16MB caching */
		switch (epson_machine_id) {
		case 0x34:	/* PC486HX */
		case 0x35:	/* PC486HG */
		case 0x3B:	/* PC486HA */
			/* Cache control start */
			outb(0x43f, 0x42);
			outw(0xc40, 0x0033);

			/* Disable 0xF00000-0xFFFFFF */
			outb(0xc48, 0x49); outb(0xc4c, 0x00);
			outb(0xc48, 0x48); outb(0xc4c, 0xf0);
			outb(0xc48, 0x4d); outb(0xc4c, 0x00);
			outb(0xc48, 0x4c); outb(0xc4c, 0xff);
			outb(0xc48, 0x4f); outb(0xc4c, 0x00);

			/* Cache control end */
			outb(0x43f, 0x40);
			break;

		case 0x2B:	/* PC486GR/GF */
		case 0x30:	/* PC486P */
		case 0x31:	/* PC486GRSuper */
		case 0x32:	/* PC486GR+ */
		case 0x37:	/* PC486SE */
		case 0x38:	/* PC486SR */
			/* Disable 0xF00000-0xFFFFFF */
			outb(0x43f, 0x42);
			outb(0x467, 0xe0);
			outb(0x567, 0xd8);

			outb(0x43f, 0x40);
			outb(0x467, 0xe0);
			outb(0x567, 0xe0);
			break;
		}

		/* Disable 15MB-16MB RAM and enable memory window */
		outb(0x43b, inb(0x43b) & 0xfd);	/* clear bit1 */
	}
}
#endif

void init_cpu_accel_mem(void)
{
	int target_page;
	/*
	 * Certain 'CPU accelerator' supports over 16MB memory on
	 * the machines whose BIOS doesn't store true size.  
	 * To support this, we don't trust BIOS values if Maxmem < 4096.
	 */
	if (Maxmem < 4096) {
		for (target_page = ptoa(4096);		/* 16MB */
			 target_page < ptoa(32768);		/* 128MB */
			 target_page += 256 * PAGE_SIZE	/* 1MB step */) {
			int tmp, page_bad = FALSE, OrigMaxmem = Maxmem;

			*(int *)CMAP1 = PG_V | PG_RW | PG_N | target_page;
			pmap_update();

			tmp = *(int *)CADDR1;
			/*
			 * Test for alternating 1's and 0's
			 */
			*(volatile int *)CADDR1 = 0xaaaaaaaa;
			if (*(volatile int *)CADDR1 != 0xaaaaaaaa) {
				page_bad = TRUE;
			}
			/*
			 * Test for alternating 0's and 1's
			 */
			*(volatile int *)CADDR1 = 0x55555555;
			if (*(volatile int *)CADDR1 != 0x55555555) {
				page_bad = TRUE;
			}
			/*
			 * Test for all 1's
			 */
			*(volatile int *)CADDR1 = 0xffffffff;
			if (*(volatile int *)CADDR1 != 0xffffffff) {
				page_bad = TRUE;
			}
			/*
			 * Test for all 0's
			 */
			*(volatile int *)CADDR1 = 0x0;
			if (*(volatile int *)CADDR1 != 0x0) {
				/*
				 * test of page failed
				 */
				page_bad = TRUE;
			}
			/*
			 * Restore original value.
			 */
			*(int *)CADDR1 = tmp;
			if (page_bad == TRUE) {
				if (target_page > ptoa(4096))
					Maxmem = atop(target_page);
				else
					Maxmem = OrigMaxmem;

				break;
			}
		}
		*(int *)CMAP1 = 0;
		pmap_update();

		/* XXX */
		if (Maxmem > 3840) {
			Maxmem_under16M = 3840;
			if (Maxmem < 4096) {
				Maxmem = 3840;
			}
		}
	}
}

int dma_init_flag = 1;	/* dummy */

void init_pc98_dmac(void)
{
	outb(0x439, (inb(0x439) & 0xfb)); /* DMA Accsess Control over 1MB */
	outb(0x29, (0x0c | 0));	/* Bank Mode Reg. 16M mode */
	outb(0x29, (0x0c | 1));	/* Bank Mode Reg. 16M mode */
	outb(0x29, (0x0c | 2));	/* Bank Mode Reg. 16M mode */
	outb(0x29, (0x0c | 3));	/* Bank Mode Reg. 16M mode */
	outb(0x11, 0x50);	/* PC98 must be 0x40 */
}
