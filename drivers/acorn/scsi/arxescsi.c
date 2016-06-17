/*
 * linux/arch/arm/drivers/scsi/arxescsi.c
 *
 * Copyright (C) 1997-2000 Russell King, Stefan Hanske
 *
 * This driver is based on experimentation.  Hence, it may have made
 * assumptions about the particular card that I have available, and
 * may not be reliable!
 *
 * Changelog:
 *  30-08-1997	RMK	0.0.0	Created, READONLY version as cumana_2.c
 *  22-01-1998	RMK	0.0.1	Updated to 2.1.80
 *  15-04-1998	RMK	0.0.1	Only do PIO if FAS216 will allow it.
 *  11-06-1998 	SH	0.0.2   Changed to support ARXE 16-bit SCSI card
 *				enabled writing
 *  01-01-2000	SH	0.1.0   Added *real* pseudo dma writing
 *				(arxescsi_pseudo_dma_write)
 *  02-04-2000	RMK	0.1.1	Updated for new error handling code.
 *  22-10-2000  SH		Updated for new registering scheme.
 */
#include <linux/module.h>
#include <linux/blk.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/unistd.h>
#include <linux/stat.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/ecard.h>

#include "../../scsi/sd.h"
#include "../../scsi/hosts.h"
#include <scsi/scsicam.h>
#include "fas216.h"
#include "scsi.h"

struct arxescsi_info {
	FAS216_Info		info;
	struct expansion_card	*ec;
	unsigned int		dmaarea;	/* Pseudo DMA area	*/
};

#define DMADATA_OFFSET	(0x200/4)

#define DMASTAT_OFFSET	(0x600/4)
#define DMASTAT_DRQ	(1 << 0)

#define CSTATUS_IRQ	(1 << 0)

/*
 * List of devices that the driver will recognise
 */
#define ARXESCSI_LIST		{ MANU_ARXE, PROD_ARXE_SCSI }

/*
 * Version
 */
#define VERSION "1.10 (22/01/2003 2.4.19-rmk5)"

/*
 * Function: int arxescsi_dma_setup(host, SCpnt, direction, min_type)
 * Purpose : initialises DMA/PIO
 * Params  : host      - host
 *	     SCpnt     - command
 *	     direction - DMA on to/off of card
 *	     min_type  - minimum DMA support that we must have for this transfer
 * Returns : 0 if we should not set CMD_WITHDMA for transfer info command
 */
static fasdmatype_t
arxescsi_dma_setup(struct Scsi_Host *host, Scsi_Pointer *SCp,
		       fasdmadir_t direction, fasdmatype_t min_type)
{
	/*
	 * We don't do real DMA
	 */
	return fasdma_pseudo;
}



/* Faster transfer routines, written by SH to speed up the loops */

static __inline__ unsigned char getb(unsigned int address, unsigned int reg)
{
	unsigned char value;

	__asm__ __volatile__(
	"ldrb	%0, [%1, %2, lsl #5]"
	: "=r" (value)
	: "r" (address), "r" (reg) );
	return value;
}

static __inline__ unsigned int getw(unsigned int address, unsigned int reg)
{
	unsigned int value;
	
	__asm__ __volatile__(
	"ldr	%0, [%1, %2, lsl #5]\n\t"
	"mov	%0, %0, lsl #16\n\t"
	"mov	%0, %0, lsr #16"
	: "=r" (value)
	: "r" (address), "r" (reg) );
	return value;
}

static __inline__ void putw(unsigned int address, unsigned int reg, unsigned long value)
{
	__asm__ __volatile__(
	"mov	%0, %0, lsl #16\n\t"
	"str	%0, [%1, %2, lsl #5]"
	:
	: "r" (value), "r" (address), "r" (reg) );
}

void arxescsi_pseudo_dma_write(unsigned char *addr, unsigned int io)
{
       __asm__ __volatile__(
       "               stmdb   sp!, {r0-r12}\n"
       "               mov     r3, %0\n"
       "               mov     r1, %1\n"
       "               add     r2, r1, #512\n"
       "               mov     r4, #256\n"
       ".loop_1:       ldmia   r3!, {r6, r8, r10, r12}\n"
       "               mov     r5, r6, lsl #16\n"
       "               mov     r7, r8, lsl #16\n"
       ".loop_2:       ldrb    r0, [r1, #1536]\n"
       "               tst     r0, #1\n"
       "               beq     .loop_2\n"
       "               stmia   r2, {r5-r8}\n\t"
       "               mov     r9, r10, lsl #16\n"
       "               mov     r11, r12, lsl #16\n"
       ".loop_3:       ldrb    r0, [r1, #1536]\n"
       "               tst     r0, #1\n"
       "               beq     .loop_3\n"
       "               stmia   r2, {r9-r12}\n"
       "               subs    r4, r4, #16\n"
       "               bne     .loop_1\n"
       "               ldmia   sp!, {r0-r12}\n"
       :
       : "r" (addr), "r" (io) );
}

/*
 * Function: int arxescsi_dma_pseudo(host, SCpnt, direction, transfer)
 * Purpose : handles pseudo DMA
 * Params  : host      - host
 *	     SCpnt     - command
 *	     direction - DMA on to/off of card
 *	     transfer  - minimum number of bytes we expect to transfer
 */
void arxescsi_dma_pseudo(struct Scsi_Host *host, Scsi_Pointer *SCp,
			fasdmadir_t direction, int transfer)
{
	struct arxescsi_info *info = (struct arxescsi_info *)host->hostdata;
	unsigned int length, io, error=0;
	unsigned char *addr;

	length = SCp->this_residual;
	addr = SCp->ptr;
	io = __ioaddr(host->io_port);

	if (direction == DMA_OUT) {
		unsigned int word;
		while (length > 256) {
			if (getb(io, 4) & STAT_INT) {
				error=1;
				break;
			}
			arxescsi_pseudo_dma_write(addr, io);
			addr += 256;
			length -= 256;
		}

		if (!error)
			while (length > 0) {
				if (getb(io, 4) & STAT_INT)
					break;
	 
				if (!(getb(io, 48) & DMASTAT_DRQ))
					continue;

				word = *addr | *(addr + 1) << 8;

				putw(io, 16, word);
				if (length > 1) {
					addr += 2;
					length -= 2;
				} else {
					addr += 1;
					length -= 1;
				}
			}
	}
	else {
		if (transfer && (transfer & 255)) {
			while (length >= 256) {
				if (getb(io, 4) & STAT_INT) {
					error=1;
					break;
				}
	    
				if (!(getb(io, 48) & DMASTAT_DRQ))
					continue;

				insw(info->dmaarea, addr, 256 >> 1);
				addr += 256;
				length -= 256;
			}
		}

		if (!(error))
			while (length > 0) {
				unsigned long word;

				if (getb(io, 4) & STAT_INT)
					break;

				if (!(getb(io, 48) & DMASTAT_DRQ))
					continue;

				word = getw(io, 16);
				*addr++ = word;
				if (--length > 0) {
					*addr++ = word >> 8;
					length --;
				}
			}
	}
}

/*
 * Function: int arxescsi_dma_stop(host, SCpnt)
 * Purpose : stops DMA/PIO
 * Params  : host  - host
 *	     SCpnt - command
 */
static void arxescsi_dma_stop(struct Scsi_Host *host, Scsi_Pointer *SCp)
{
	/*
	 * no DMA to stop
	 */
}

/*
 * Function: const char *arxescsi_info(struct Scsi_Host * host)
 * Purpose : returns a descriptive string about this interface,
 * Params  : host - driver host structure to return info for.
 * Returns : pointer to a static buffer containing null terminated string.
 */
const char *arxescsi_info(struct Scsi_Host *host)
{
	struct arxescsi_info *info = (struct arxescsi_info *)host->hostdata;
	static char string[150];

	sprintf(string, "%s (%s) in slot %d v%s",
		host->hostt->name, info->info.scsi.type, info->ec->slot_no,
		VERSION);

	return string;
}

/*
 * Function: int arxescsi_proc_info(char *buffer, char **start, off_t offset,
 *					 int length, int host_no, int inout)
 * Purpose : Return information about the driver to a user process accessing
 *	     the /proc filesystem.
 * Params  : buffer - a buffer to write information to
 *	     start  - a pointer into this buffer set by this routine to the start
 *		      of the required information.
 *	     offset - offset into information that we have read upto.
 *	     length - length of buffer
 *	     host_no - host number to return information for
 *	     inout  - 0 for reading, 1 for writing.
 * Returns : length of data written to buffer.
 */
int arxescsi_proc_info(char *buffer, char **start, off_t offset,
			    int length, int host_no, int inout)
{
	int pos, begin;
	struct Scsi_Host *host;
	struct arxescsi_info *info;
	Scsi_Device *scd;

	host = scsi_host_hn_get(host_no);
	if (!host)
		return 0;

	info = (struct arxescsi_info *)host->hostdata;
	if (inout == 1)
		return -EINVAL;

	begin = 0;
	pos = sprintf(buffer, "ARXE 16-bit SCSI driver v%s\n", VERSION);
	pos += fas216_print_host(&info->info, buffer + pos);
	pos += fas216_print_stats(&info->info, buffer + pos);

	pos += sprintf (buffer+pos, "\nAttached devices:\n");

	for (scd = host->host_queue; scd; scd = scd->next) {
		pos += fas216_print_device(&info->info, scd, buffer + pos);

		if (pos + begin < offset) {
			begin += pos;
			pos = 0;
		}
		if (pos + begin > offset + length)
			break;
	}

	*start = buffer + (offset - begin);
	pos -= offset - begin;
	if (pos > length)
		pos = length;

	return pos;
}

static int arxescsi_probe(struct expansion_card *ec);

/*
 * Function: int arxescsi_detect(Scsi_Host_Template * tpnt)
 * Purpose : initialises ARXE SCSI driver
 * Params  : tpnt - template for this SCSI adapter
 * Returns : >0 if host found, 0 otherwise.
 */
static int arxescsi_detect(Scsi_Host_Template *tpnt)
{
	static const card_ids arxescsi_cids[] = { ARXESCSI_LIST, { 0xffff, 0xffff} };
	int count = 0, ret;
  
	ecard_startfind();

	while (1) {
		struct expansion_card *ec;
		ec = ecard_find(0, arxescsi_cids);
		if (!ec)
			break;

		ecard_claim(ec);
		ret = arxescsi_probe(ec);
		if (ret) {
			ecard_release(ec);
			break;
		}
		++count;
	}
	return count;
}

static void arxescsi_remove(struct Scsi_Host *host);

/*
 * Function: int arxescsi_release(struct Scsi_Host * host)
 * Purpose : releases all resources used by this adapter
 * Params  : host - driver host structure to return info for.
 * Returns : nothing
 */
static int arxescsi_release(struct Scsi_Host *host)
{
	arxescsi_remove(host);
	return 0;
}

static Scsi_Host_Template arxescsi_template = {
	.module				= THIS_MODULE,
	.proc_info			= arxescsi_proc_info,
	.name				= "ARXE SCSI card",
	.detect				= arxescsi_detect,
	.release			= arxescsi_release,
	.info				= arxescsi_info,
	.bios_param			= scsicam_bios_param,
	.command			= fas216_command,
	.queuecommand			= fas216_queue_command,
	.eh_host_reset_handler		= fas216_eh_host_reset,
	.eh_bus_reset_handler		= fas216_eh_bus_reset,
	.eh_device_reset_handler	= fas216_eh_device_reset,
	.eh_abort_handler		= fas216_eh_abort,
	.use_new_eh_code		= 1,

	.can_queue			= 0,
	.this_id			= 7,
	.sg_tablesize			= SG_ALL,
	.cmd_per_lun			= 1,
	.use_clustering			= DISABLE_CLUSTERING,
	.proc_name			= "arxescsi",
};

static int
arxescsi_probe(struct expansion_card *ec)
{
	struct Scsi_Host *host;
	struct arxescsi_info *info;
	unsigned long base;
	int ret;

	base = ecard_address(ec, ECARD_MEMC, 0) + 0x0800;

	if (!request_region(base, 512, "arxescsi")) {
		ret = -EBUSY;
		goto out;
	}

	host = scsi_register(&arxescsi_template, sizeof(struct arxescsi_info));
	if (!host) {
		ret = -ENOMEM;
		goto out_region;
	}

	host->io_port = base;
	host->irq = NO_IRQ;
	host->dma_channel = NO_DMA;

	info = (struct arxescsi_info *)host->hostdata;
	info->ec = ec;
	info->dmaarea = base + DMADATA_OFFSET;

	info->info.scsi.io_port		= host->io_port;
	info->info.scsi.irq		= NO_IRQ;
	info->info.scsi.io_shift	= 3;
	info->info.ifcfg.clockrate	= 24; /* MHz */
	info->info.ifcfg.select_timeout = 255;
	info->info.ifcfg.asyncperiod	= 200; /* ns */
	info->info.ifcfg.sync_max_depth	= 0;
	info->info.ifcfg.cntl3		= CNTL3_FASTSCSI | CNTL3_FASTCLK;
	info->info.ifcfg.disconnect_ok	= 0;
	info->info.ifcfg.wide_max_size	= 0;
	info->info.ifcfg.capabilities	= FASCAP_PSEUDODMA;
	info->info.dma.setup		= arxescsi_dma_setup;
	info->info.dma.pseudo		= arxescsi_dma_pseudo;
	info->info.dma.stop		= arxescsi_dma_stop;

	ec->irqaddr = (unsigned char *)ioaddr(base);
	ec->irqmask = CSTATUS_IRQ;

	ret = fas216_init(host);
	if (ret)
		goto out_unregister;

	ret = fas216_add(host);
	if (ret == 0)
		goto out;

	fas216_release(host);
 out_unregister:
	scsi_unregister(host);
 out_region:
	release_region(base, 512);
 out:
	return ret;
}

static void arxescsi_remove(struct Scsi_Host *host)
{
	struct arxescsi_info *info;

	info = (struct arxescsi_info *)host->hostdata;

	fas216_remove(host);

	release_region(host->io_port, 512);

	ecard_release(info->ec);
	fas216_release(host);
}

static int __init init_arxe_scsi_driver(void)
{
	scsi_register_module(MODULE_SCSI_HA, &arxescsi_template);
	if (arxescsi_template.present)
		return 0;

	scsi_unregister_module(MODULE_SCSI_HA, &arxescsi_template);
	return -ENODEV;
}

static void __exit exit_arxe_scsi_driver(void)
{
	scsi_unregister_module(MODULE_SCSI_HA, &arxescsi_template);
}

module_init(init_arxe_scsi_driver);
module_exit(exit_arxe_scsi_driver);

MODULE_AUTHOR("Stefan Hanske");
MODULE_DESCRIPTION("ARXESCSI driver for Acorn machines");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;
