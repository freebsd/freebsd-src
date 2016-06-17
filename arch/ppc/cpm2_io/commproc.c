/*
 * General Purpose functions for the global management of the
 * 8260 Communication Processor Module.
 * Copyright (c) 1999 Dan Malek (dmalek@jlc.net)
 * Copyright (c) 2000 MontaVista Software, Inc (source@mvista.com)
 *	2.3.99 Updates
 *
 * In addition to the individual control of the communication
 * channels, there are a few functions that globally affect the
 * communication processor.
 *
 * Buffer descriptors must be allocated from the dual ported memory
 * space.  The allocator for that is here.  When the communication
 * process is reset, we reclaim the memory available.  There is
 * currently no deallocator for this memory.
 */
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/bootmem.h>
#include <asm/irq.h>
#include <asm/mpc8260.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/immap_cpm2.h>
#include <asm/cpm2.h>

static	uint	dp_alloc_base;	/* Starting offset in DP ram */
static	uint	dp_alloc_top;	/* Max offset + 1 */
static	uint	host_buffer;	/* One page of host buffer */
static	uint	host_end;	/* end + 1 */
cpm_cpm2_t	*cpmp;		/* Pointer to comm processor space */

/* We allocate this here because it is used almost exclusively for
 * the communication processor devices.
 */
cpm2_map_t		*cpm2_immr;

void
cpm2_reset(void)
{
	volatile cpm2_map_t	 *imp;
	volatile cpm_cpm2_t	*commproc;
	uint			vpgaddr;

	cpm2_immr = imp = (volatile cpm2_map_t *)CPM_MAP_ADDR;
	commproc = &imp->im_cpm;

	/* Reclaim the DP memory for our use.
	*/
	dp_alloc_base = CPM_DATAONLY_BASE;
	dp_alloc_top = dp_alloc_base + CPM_DATAONLY_SIZE;

	/* Set the host page for allocation.
	*/
	host_buffer =
		(uint) alloc_bootmem_pages(PAGE_SIZE * NUM_CPM_HOST_PAGES);
	host_end = host_buffer + (PAGE_SIZE * NUM_CPM_HOST_PAGES);

	vpgaddr = host_buffer;

	/* Tell everyone where the comm processor resides.
	*/
	cpmp = (cpm_cpm2_t *)commproc;
}

/* Allocate some memory from the dual ported ram.
 * To help protocols with object alignment restrictions, we do that
 * if they ask.
 */
uint
cpm2_dpalloc(uint size, uint align)
{
	uint	retloc;
	uint	align_mask, off;
	uint	savebase;

	align_mask = align - 1;
	savebase = dp_alloc_base;

	if ((off = (dp_alloc_base & align_mask)) != 0)
		dp_alloc_base += (align - off);

	if ((dp_alloc_base + size) >= dp_alloc_top) {
		dp_alloc_base = savebase;
		return(CPM_DP_NOSPACE);
	}

	retloc = dp_alloc_base;
	dp_alloc_base += size;

	return(retloc);
}

/* We also own one page of host buffer space for the allocation of
 * UART "fifos" and the like.
 */
uint
cpm2_hostalloc(uint size, uint align)
{
	uint	retloc;
	uint	align_mask, off;
	uint	savebase;

	align_mask = align - 1;
	savebase = host_buffer;

	if ((off = (host_buffer & align_mask)) != 0)
		host_buffer += (align - off);

	if ((host_buffer + size) >= host_end) {
		host_buffer = savebase;
		return(0);
	}

	retloc = host_buffer;
	host_buffer += size;

	return(retloc);
}

/* Set a baud rate generator.  This needs lots of work.  There are
 * eight BRGs, which can be connected to the CPM channels or output
 * as clocks.  The BRGs are in two different block of internal
 * memory mapped space.
 * The baud rate clock is the system clock divided by something.
 * It was set up long ago during the initial boot phase and is
 * is given to us.
 * Baud rate clocks are zero-based in the driver code (as that maps
 * to port numbers).  Documentation uses 1-based numbering.
 */
#define BRG_INT_CLK	(((bd_t *)__res)->bi_brgfreq)
#define BRG_UART_CLK	(BRG_INT_CLK/16)

/* This function is used by UARTS, or anything else that uses a 16x
 * oversampled clock.
 */
void
cpm2_setbrg(uint brg, uint rate)
{
	volatile uint	*bp;

	/* This is good enough to get SMCs running.....
	*/
	if (brg < 4) {
		bp = (uint *)&cpm2_immr->im_brgc1;
	}
	else {
		bp = (uint *)&cpm2_immr->im_brgc5;
		brg -= 4;
	}
	bp += brg;
	*bp = ((BRG_UART_CLK / rate) << 1) | CPM_BRG_EN;
}

/* This function is used to set high speed synchronous baud rate
 * clocks.
 */
void
cpm2_fastbrg(uint brg, uint rate, int div16)
{
	volatile uint	*bp;

	if (brg < 4) {
		bp = (uint *)&cpm2_immr->im_brgc1;
	}
	else {
		bp = (uint *)&cpm2_immr->im_brgc5;
		brg -= 4;
	}
	bp += brg;
	*bp = ((BRG_INT_CLK / rate) << 1) | CPM_BRG_EN;
	if (div16)
		*bp |= CPM_BRG_DIV16;
}
