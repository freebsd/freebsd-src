/* -*- mode: c; c-basic-offset: 8 -*- */

/* NCR (or Symbios) 53c700 and 53c700-66 Driver
 *
 * Copyright (C) 2001 by James.Bottomley@HansenPartnership.com
**-----------------------------------------------------------------------------
**  
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**-----------------------------------------------------------------------------
 */

/* Notes:
 *
 * This driver is designed exclusively for these chips (virtually the
 * earliest of the scripts engine chips).  They need their own drivers
 * because they are missing so many of the scripts and snazzy register
 * features of their elder brothers (the 710, 720 and 770).
 *
 * The 700 is the lowliest of the line, it can only do async SCSI.
 * The 700-66 can at least do synchronous SCSI up to 10MHz.
 * 
 * The 700 chip has no host bus interface logic of its own.  However,
 * it is usually mapped to a location with well defined register
 * offsets.  Therefore, if you can determine the base address and the
 * irq your board incorporating this chip uses, you can probably use
 * this driver to run it (although you'll probably have to write a
 * minimal wrapper for the purpose---see the NCR_D700 driver for
 * details about how to do this).
 *
 *
 * TODO List:
 *
 * 1. Better statistics in the proc fs
 *
 * 2. Implement message queue (queues SCSI messages like commands) and make
 *    the abort and device reset functions use them.
 * */

/* CHANGELOG
 *
 * Version 2.8
 *
 * Fixed bad bug affecting tag starvation processing (previously the
 * driver would hang the system if too many tags starved.  Also fixed
 * bad bug having to do with 10 byte command processing and REQUEST
 * SENSE (the command would loop forever getting a transfer length
 * mismatch in the CMD phase).
 *
 * Version 2.7
 *
 * Fixed scripts problem which caused certain devices (notably CDRWs)
 * to hang on initial INQUIRY.  Updated NCR_700_readl/writel to use
 * __raw_readl/writel for parisc compatibility (Thomas
 * Bogendoerfer). Added missing SCp->request_bufflen initialisation
 * for sense requests (Ryan Bradetich).
 *
 * Version 2.6
 *
 * Following test of the 64 bit parisc kernel by Richard Hirst,
 * several problems have now been corrected.  Also adds support for
 * consistent memory allocation.
 *
 * Version 2.5
 * 
 * More Compatibility changes for 710 (now actually works).  Enhanced
 * support for odd clock speeds which constrain SDTR negotiations.
 * correct cacheline separation for scsi messages and status for
 * incoherent architectures.  Use of the pci mapping functions on
 * buffers to begin support for 64 bit drivers.
 *
 * Version 2.4
 *
 * Added support for the 53c710 chip (in 53c700 emulation mode only---no 
 * special 53c710 instructions or registers are used).
 *
 * Version 2.3
 *
 * More endianness/cache coherency changes.
 *
 * Better bad device handling (handles devices lying about tag
 * queueing support and devices which fail to provide sense data on
 * contingent allegiance conditions)
 *
 * Many thanks to Richard Hirst <rhirst@linuxcare.com> for patiently
 * debugging this driver on the parisc architecture and suggesting
 * many improvements and bug fixes.
 *
 * Thanks also go to Linuxcare Inc. for providing several PARISC
 * machines for me to debug the driver on.
 *
 * Version 2.2
 *
 * Made the driver mem or io mapped; added endian invariance; added
 * dma cache flushing operations for architectures which need it;
 * added support for more varied clocking speeds.
 *
 * Version 2.1
 *
 * Initial modularisation from the D700.  See NCR_D700.c for the rest of
 * the changelog.
 * */
#define NCR_700_VERSION "2.8"

#include <linux/config.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/mca.h>
#include <asm/dma.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/byteorder.h>
#include <linux/blk.h>
#include <linux/module.h>
#include <linux/pci.h>

#include "scsi.h"
#include "hosts.h"
#include "constants.h"

#include "53c700.h"

/* NOTE: For 64 bit drivers there are points in the code where we use
 * a non dereferenceable pointer to point to a structure in dma-able
 * memory (which is 32 bits) so that we can use all of the structure
 * operations but take the address at the end.  This macro allows us
 * to truncate the 64 bit pointer down to 32 bits without the compiler
 * complaining */
#define to32bit(x)	((__u32)((unsigned long)(x)))

#ifdef NCR_700_DEBUG
#define STATIC
#else
#define STATIC static
#endif

MODULE_AUTHOR("James Bottomley");
MODULE_DESCRIPTION("53c700 and 53c700-66 Driver");
MODULE_LICENSE("GPL");

/* This is the script */
#include "53c700_d.h"


STATIC int NCR_700_queuecommand(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
STATIC int NCR_700_abort(Scsi_Cmnd * SCpnt);
STATIC int NCR_700_bus_reset(Scsi_Cmnd * SCpnt);
STATIC int NCR_700_dev_reset(Scsi_Cmnd * SCpnt);
STATIC int NCR_700_host_reset(Scsi_Cmnd * SCpnt);
STATIC int NCR_700_proc_directory_info(char *, char **, off_t, int, int, int);
STATIC void NCR_700_chip_setup(struct Scsi_Host *host);
STATIC void NCR_700_chip_reset(struct Scsi_Host *host);

static char *NCR_700_phase[] = {
	"",
	"after selection",
	"before command phase",
	"after command phase",
	"after status phase",
	"after data in phase",
	"after data out phase",
	"during data phase",
};

static char *NCR_700_condition[] = {
	"",
	"NOT MSG_OUT",
	"UNEXPECTED PHASE",
	"NOT MSG_IN",
	"UNEXPECTED MSG",
	"MSG_IN",
	"SDTR_MSG RECEIVED",
	"REJECT_MSG RECEIVED",
	"DISCONNECT_MSG RECEIVED",
	"MSG_OUT",
	"DATA_IN",
	
};

static char *NCR_700_fatal_messages[] = {
	"unexpected message after reselection",
	"still MSG_OUT after message injection",
	"not MSG_IN after selection",
	"Illegal message length received",
};

static char *NCR_700_SBCL_bits[] = {
	"IO ",
	"CD ",
	"MSG ",
	"ATN ",
	"SEL ",
	"BSY ",
	"ACK ",
	"REQ ",
};

static char *NCR_700_SBCL_to_phase[] = {
	"DATA_OUT",
	"DATA_IN",
	"CMD_OUT",
	"STATE",
	"ILLEGAL PHASE",
	"ILLEGAL PHASE",
	"MSG OUT",
	"MSG IN",
};

static __u8 NCR_700_SDTR_msg[] = {
	0x01,			/* Extended message */
	0x03,			/* Extended message Length */
	0x01,			/* SDTR Extended message */
	NCR_700_MIN_PERIOD,
	NCR_700_MAX_OFFSET
};

struct Scsi_Host * __init
NCR_700_detect(Scsi_Host_Template *tpnt,
	       struct NCR_700_Host_Parameters *hostdata)
{
	dma_addr_t pScript, pMemory, pSlots;
	__u8 *memory;
	__u32 *script;
	struct Scsi_Host *host;
	static int banner = 0;
	int j;

#ifdef CONFIG_53C700_USE_CONSISTENT
	memory = pci_alloc_consistent(hostdata->pci_dev, TOTAL_MEM_SIZE,
				      &pMemory);
	hostdata->consistent = 1;
	if(memory == NULL ) {
		printk(KERN_WARNING "53c700: consistent memory allocation failed\n");
#endif
		memory = kmalloc(TOTAL_MEM_SIZE, GFP_KERNEL);
		if(memory == NULL) {
			printk(KERN_ERR "53c700: Failed to allocate memory for driver, detatching\n");
			return NULL;
		}
		pMemory = pci_map_single(hostdata->pci_dev, memory,
					 TOTAL_MEM_SIZE, PCI_DMA_BIDIRECTIONAL);
#ifdef CONFIG_53C700_USE_CONSISTENT
		hostdata->consistent = 0;
	}
#endif
	script = (__u32 *)memory;
	pScript = pMemory;
	hostdata->msgin = memory + MSGIN_OFFSET;
	hostdata->msgout = memory + MSGOUT_OFFSET;
	hostdata->status = memory + STATUS_OFFSET;
	hostdata->slots = (struct NCR_700_command_slot *)(memory + SLOTS_OFFSET);
		
	pSlots = pMemory + SLOTS_OFFSET;

	/* Fill in the missing routines from the host template */
	tpnt->queuecommand = NCR_700_queuecommand;
	tpnt->eh_abort_handler = NCR_700_abort;
	tpnt->eh_device_reset_handler = NCR_700_dev_reset;
	tpnt->eh_bus_reset_handler = NCR_700_bus_reset;
	tpnt->eh_host_reset_handler = NCR_700_host_reset;
	tpnt->can_queue = NCR_700_COMMAND_SLOTS_PER_HOST;
	tpnt->sg_tablesize = NCR_700_SG_SEGMENTS;
	tpnt->cmd_per_lun = NCR_700_MAX_TAGS;
	tpnt->use_clustering = DISABLE_CLUSTERING;
	tpnt->use_new_eh_code = 1;
	tpnt->proc_info = NCR_700_proc_directory_info;
	
	if(tpnt->name == NULL)
		tpnt->name = "53c700";
	if(tpnt->proc_name == NULL)
		tpnt->proc_name = "53c700";
	

	if((host = scsi_register(tpnt, 4)) == NULL)
		return NULL;
	memset(hostdata->slots, 0, sizeof(struct NCR_700_command_slot)
	       * NCR_700_COMMAND_SLOTS_PER_HOST);
	for(j = 0; j < NCR_700_COMMAND_SLOTS_PER_HOST; j++) {
		dma_addr_t offset = (dma_addr_t)((unsigned long)&hostdata->slots[j].SG[0]
					  - (unsigned long)&hostdata->slots[0].SG[0]);
		hostdata->slots[j].pSG = (struct NCR_700_SG_List *)((unsigned long)(pSlots + offset));
		if(j == 0)
			hostdata->free_list = &hostdata->slots[j];
		else
			hostdata->slots[j-1].ITL_forw = &hostdata->slots[j];
		hostdata->slots[j].state = NCR_700_SLOT_FREE;
	}

	for(j = 0; j < sizeof(SCRIPT)/sizeof(SCRIPT[0]); j++) {
		script[j] = bS_to_host(SCRIPT[j]);
	}

	/* adjust all labels to be bus physical */
	for(j = 0; j < PATCHES; j++) {
		script[LABELPATCHES[j]] = bS_to_host(pScript + SCRIPT[LABELPATCHES[j]]);
	}
	/* now patch up fixed addresses. */
	script_patch_32(script, MessageLocation,
			pScript + MSGOUT_OFFSET);
	script_patch_32(script, StatusAddress,
			pScript + STATUS_OFFSET);
	script_patch_32(script, ReceiveMsgAddress,
			pScript + MSGIN_OFFSET);

	hostdata->script = script;
	hostdata->pScript = pScript;
	NCR_700_dma_cache_wback((unsigned long)script, sizeof(SCRIPT));
	hostdata->state = NCR_700_HOST_FREE;
	spin_lock_init(&hostdata->lock);
	hostdata->cmd = NULL;
	host->max_id = 7;
	host->max_lun = NCR_700_MAX_LUNS;
	host->unique_id = hostdata->base;
	host->base = hostdata->base;
	host->hostdata[0] = (unsigned long)hostdata;
	/* kick the chip */
	NCR_700_writeb(0xff, host, CTEST9_REG);
	if(hostdata->chip710) 
		hostdata->rev = (NCR_700_readb(host, CTEST8_REG)>>4) & 0x0f;
	else
		hostdata->rev = (NCR_700_readb(host, CTEST7_REG)>>4) & 0x0f;
	hostdata->fast = (NCR_700_readb(host, CTEST9_REG) == 0);
	if(banner == 0) {
		printk(KERN_NOTICE "53c700: Version " NCR_700_VERSION " By James.Bottomley@HansenPartnership.com\n");
		banner = 1;
	}
	printk(KERN_NOTICE "scsi%d: %s rev %d %s\n", host->host_no,
	       hostdata->chip710 ? "53c710" : 
	       (hostdata->fast ? "53c700-66" : "53c700"),
	       hostdata->rev, hostdata->differential ?
	       "(Differential)" : "");
	/* reset the chip */
	NCR_700_chip_reset(host);

	return host;
}

int
NCR_700_release(struct Scsi_Host *host)
{
	struct NCR_700_Host_Parameters *hostdata = 
		(struct NCR_700_Host_Parameters *)host->hostdata[0];

#ifdef CONFIG_53C700_USE_CONSISTENT
	if(hostdata->consistent) {
		pci_free_consistent(hostdata->pci_dev, TOTAL_MEM_SIZE,
				    hostdata->script, hostdata->pScript);
	} else {
#endif
		pci_unmap_single(hostdata->pci_dev, hostdata->pScript,
				 TOTAL_MEM_SIZE, PCI_DMA_BIDIRECTIONAL);
		kfree(hostdata->script);
#ifdef CONFIG_53C700_USE_CONSISTENT
	}
#endif
	return 1;
}

static inline __u8
NCR_700_identify(int can_disconnect, __u8 lun)
{
	return IDENTIFY_BASE |
		((can_disconnect) ? 0x40 : 0) |
		(lun & NCR_700_LUN_MASK);
}

/*
 * Function : static int data_residual (Scsi_Host *host)
 *
 * Purpose : return residual data count of what's in the chip.  If you
 * really want to know what this function is doing, it's almost a
 * direct transcription of the algorithm described in the 53c710
 * guide, except that the DBC and DFIFO registers are only 6 bits
 * wide on a 53c700.
 *
 * Inputs : host - SCSI host */
static inline int
NCR_700_data_residual (struct Scsi_Host *host) {
	struct NCR_700_Host_Parameters *hostdata = 
		(struct NCR_700_Host_Parameters *)host->hostdata[0];
	int count, synchronous = 0;
	unsigned int ddir;

	if(hostdata->chip710) {
		count = ((NCR_700_readb(host, DFIFO_REG) & 0x7f) -
			 (NCR_700_readl(host, DBC_REG) & 0x7f)) & 0x7f;
	} else {
		count = ((NCR_700_readb(host, DFIFO_REG) & 0x3f) -
			 (NCR_700_readl(host, DBC_REG) & 0x3f)) & 0x3f;
	}
	
	if(hostdata->fast)
		synchronous = NCR_700_readb(host, SXFER_REG) & 0x0f;
	
	/* get the data direction */
	ddir = NCR_700_readb(host, CTEST0_REG) & 0x01;

	if (ddir) {
		/* Receive */
		if (synchronous) 
			count += (NCR_700_readb(host, SSTAT2_REG) & 0xf0) >> 4;
		else
			if (NCR_700_readb(host, SSTAT1_REG) & SIDL_REG_FULL)
				++count;
	} else {
		/* Send */
		__u8 sstat = NCR_700_readb(host, SSTAT1_REG);
		if (sstat & SODL_REG_FULL)
			++count;
		if (synchronous && (sstat & SODR_REG_FULL))
			++count;
	}
#ifdef NCR_700_DEBUG
	if(count)
		printk("RESIDUAL IS %d (ddir %d)\n", count, ddir);
#endif
	return count;
}

/* print out the SCSI wires and corresponding phase from the SBCL register
 * in the chip */
static inline char *
sbcl_to_string(__u8 sbcl)
{
	int i;
	static char ret[256];

	ret[0]='\0';
	for(i=0; i<8; i++) {
		if((1<<i) & sbcl) 
			strcat(ret, NCR_700_SBCL_bits[i]);
	}
	strcat(ret, NCR_700_SBCL_to_phase[sbcl & 0x07]);
	return ret;
}

static inline __u8
bitmap_to_number(__u8 bitmap)
{
	__u8 i;

	for(i=0; i<8 && !(bitmap &(1<<i)); i++)
		;
	return i;
}

/* Pull a slot off the free list */
STATIC struct NCR_700_command_slot *
find_empty_slot(struct NCR_700_Host_Parameters *hostdata)
{
	struct NCR_700_command_slot *slot = hostdata->free_list;

	if(slot == NULL) {
		/* sanity check */
		if(hostdata->command_slot_count != NCR_700_COMMAND_SLOTS_PER_HOST)
			printk(KERN_ERR "SLOTS FULL, but count is %d, should be %d\n", hostdata->command_slot_count, NCR_700_COMMAND_SLOTS_PER_HOST);
		return NULL;
	}

	if(slot->state != NCR_700_SLOT_FREE)
		/* should panic! */
		printk(KERN_ERR "BUSY SLOT ON FREE LIST!!!\n");
		

	hostdata->free_list = slot->ITL_forw;
	slot->ITL_forw = NULL;


	/* NOTE: set the state to busy here, not queued, since this
	 * indicates the slot is in use and cannot be run by the IRQ
	 * finish routine.  If we cannot queue the command when it
	 * is properly build, we then change to NCR_700_SLOT_QUEUED */
	slot->state = NCR_700_SLOT_BUSY;
	hostdata->command_slot_count++;
	
	return slot;
}

STATIC void 
free_slot(struct NCR_700_command_slot *slot,
	  struct NCR_700_Host_Parameters *hostdata)
{
	int hash;
	struct NCR_700_command_slot **forw, **back;


	if((slot->state & NCR_700_SLOT_MASK) != NCR_700_SLOT_MAGIC) {
		printk(KERN_ERR "53c700: SLOT %p is not MAGIC!!!\n", slot);
	}
	if(slot->state == NCR_700_SLOT_FREE) {
		printk(KERN_ERR "53c700: SLOT %p is FREE!!!\n", slot);
	}
	/* remove from queues */
	if(slot->tag != NCR_700_NO_TAG) {
		hash = hash_ITLQ(slot->cmnd->target, slot->cmnd->lun,
				 slot->tag);
		if(slot->ITLQ_forw == NULL)
			back = &hostdata->ITLQ_Hash_back[hash];
		else
			back = &slot->ITLQ_forw->ITLQ_back;

		if(slot->ITLQ_back == NULL)
			forw = &hostdata->ITLQ_Hash_forw[hash];
		else
			forw = &slot->ITLQ_back->ITLQ_forw;

		*forw = slot->ITLQ_forw;
		*back = slot->ITLQ_back;
	}
	hash = hash_ITL(slot->cmnd->target, slot->cmnd->lun);
	if(slot->ITL_forw == NULL)
		back = &hostdata->ITL_Hash_back[hash];
	else
		back = &slot->ITL_forw->ITL_back;
	
	if(slot->ITL_back == NULL)
		forw = &hostdata->ITL_Hash_forw[hash];
	else
		forw = &slot->ITL_back->ITL_forw;
	
	*forw = slot->ITL_forw;
	*back = slot->ITL_back;
	
	slot->resume_offset = 0;
	slot->cmnd = NULL;
	slot->state = NCR_700_SLOT_FREE;
	slot->ITL_forw = hostdata->free_list;
	hostdata->free_list = slot;
	hostdata->command_slot_count--;
}


/* This routine really does very little.  The command is indexed on
   the ITL and (if tagged) the ITLQ lists in _queuecommand */
STATIC void
save_for_reselection(struct NCR_700_Host_Parameters *hostdata,
		     Scsi_Cmnd *SCp, __u32 dsp)
{
	/* Its just possible that this gets executed twice */
	if(SCp != NULL) {
		struct NCR_700_command_slot *slot =
			(struct NCR_700_command_slot *)SCp->host_scribble;

		slot->resume_offset = dsp;
	}
	hostdata->state = NCR_700_HOST_FREE;
	hostdata->cmd = NULL;
}

/* Most likely nexus is the oldest in each case */
STATIC inline struct NCR_700_command_slot *
find_ITL_Nexus(struct NCR_700_Host_Parameters *hostdata, __u8 pun, __u8 lun)
{
	int hash = hash_ITL(pun, lun);
	struct NCR_700_command_slot *slot = hostdata->ITL_Hash_back[hash];
	while(slot != NULL && !(slot->cmnd->target == pun &&
				slot->cmnd->lun == lun))
		slot = slot->ITL_back;
	return slot;
}

STATIC inline struct NCR_700_command_slot *
find_ITLQ_Nexus(struct NCR_700_Host_Parameters *hostdata, __u8 pun,
		__u8 lun, __u8 tag)
{
	int hash = hash_ITLQ(pun, lun, tag);
	struct NCR_700_command_slot *slot = hostdata->ITLQ_Hash_back[hash];

	while(slot != NULL && !(slot->cmnd->target == pun 
	      && slot->cmnd->lun == lun && slot->tag == tag))
		slot = slot->ITLQ_back;

#ifdef NCR_700_TAG_DEBUG
	if(slot != NULL) {
		struct NCR_700_command_slot *n = slot->ITLQ_back;
		while(n != NULL && n->cmnd->target != pun
		      && n->cmnd->lun != lun && n->tag != tag)
			n = n->ITLQ_back;

		if(n != NULL && n->cmnd->target == pun && n->cmnd->lun == lun
		   && n->tag == tag) {
			printk(KERN_WARNING "53c700: WARNING: DUPLICATE tag %d\n",
			       tag);
		}
	}
#endif
	return slot;
}



/* This translates the SDTR message offset and period to a value
 * which can be loaded into the SXFER_REG.
 *
 * NOTE: According to SCSI-2, the true transfer period (in ns) is
 *       actually four times this period value */
STATIC inline __u8
NCR_700_offset_period_to_sxfer(struct NCR_700_Host_Parameters *hostdata,
			       __u8 offset, __u8 period)
{
	int XFERP;
	__u8 min_xferp = (hostdata->chip710
			  ? NCR_710_MIN_XFERP : NCR_700_MIN_XFERP);
	__u8 max_offset = (hostdata->chip710
			   ? NCR_710_MAX_OFFSET : NCR_700_MAX_OFFSET);
	/* NOTE: NCR_700_SDTR_msg[3] contains our offer of the minimum
	 * period.  It is set in NCR_700_chip_setup() */
	if(period < NCR_700_SDTR_msg[3]) {
		printk(KERN_WARNING "53c700: Period %dns is less than this chip's minimum, setting to %d\n", period*4, NCR_700_SDTR_msg[3]*4);
		period = NCR_700_SDTR_msg[3];
	}
	XFERP = (period*4 * hostdata->sync_clock)/1000 - 4;
	if(offset > max_offset) {
		printk(KERN_WARNING "53c700: Offset %d exceeds chip maximum, setting to %d\n",
		       offset, max_offset);
		offset = max_offset;
	}
	if(XFERP < min_xferp) {
		printk(KERN_WARNING "53c700: XFERP %d is less than minium, setting to %d\n",
		       XFERP,  min_xferp);
		XFERP =  min_xferp;
	}
	return (offset & 0x0f) | (XFERP & 0x07)<<4;
}

STATIC inline void
NCR_700_unmap(struct NCR_700_Host_Parameters *hostdata, Scsi_Cmnd *SCp,
	      struct NCR_700_command_slot *slot)
{
	if(SCp->sc_data_direction != SCSI_DATA_NONE &&
	   SCp->sc_data_direction != SCSI_DATA_UNKNOWN) {
		int pci_direction = scsi_to_pci_dma_dir(SCp->sc_data_direction);
		if(SCp->use_sg) {
			pci_unmap_sg(hostdata->pci_dev, SCp->buffer,
				     SCp->use_sg, pci_direction);
		} else {
			pci_unmap_single(hostdata->pci_dev,
					 slot->dma_handle,
					 SCp->request_bufflen,
					 pci_direction);
		}
	}
}

STATIC inline void
NCR_700_scsi_done(struct NCR_700_Host_Parameters *hostdata,
	       Scsi_Cmnd *SCp, int result)
{
	hostdata->state = NCR_700_HOST_FREE;
	hostdata->cmd = NULL;

	if(SCp != NULL) {
		struct NCR_700_command_slot *slot = 
			(struct NCR_700_command_slot *)SCp->host_scribble;
		
		NCR_700_unmap(hostdata, SCp, slot);
		pci_unmap_single(hostdata->pci_dev, slot->pCmd,
				 sizeof(SCp->cmnd), PCI_DMA_TODEVICE);
		if(SCp->cmnd[0] == REQUEST_SENSE && SCp->cmnd[6] == NCR_700_INTERNAL_SENSE_MAGIC) {
#ifdef NCR_700_DEBUG
			printk(" ORIGINAL CMD %p RETURNED %d, new return is %d sense is\n",
			       SCp, SCp->cmnd[7], result);
			print_sense("53c700", SCp);

#endif
			/* restore the old result if the request sense was
			 * successful */
			if(result == 0)
				result = SCp->cmnd[7];
			/* now restore the original command */
			memcpy((void *) SCp->cmnd, (void *) SCp->data_cmnd,
			       sizeof(SCp->data_cmnd));
			SCp->request_buffer = SCp->buffer;
			SCp->request_bufflen = SCp->bufflen;
			SCp->use_sg = SCp->old_use_sg;
			SCp->cmd_len = SCp->old_cmd_len;
			SCp->sc_data_direction = SCp->sc_old_data_direction;
			SCp->underflow = SCp->old_underflow;
			
		}

		free_slot(slot, hostdata);
#ifdef NCR_700_DEBUG
		if(NCR_700_get_depth(SCp->device) == 0 ||
		   NCR_700_get_depth(SCp->device) > NCR_700_MAX_TAGS)
			printk(KERN_ERR "Invalid depth in NCR_700_scsi_done(): %d\n",
			       NCR_700_get_depth(SCp->device));
#endif /* NCR_700_DEBUG */
		NCR_700_set_depth(SCp->device, NCR_700_get_depth(SCp->device) - 1);

		SCp->host_scribble = NULL;
		SCp->result = result;
		SCp->scsi_done(SCp);
	} else {
		printk(KERN_ERR "53c700: SCSI DONE HAS NULL SCp\n");
	}
}


STATIC void
NCR_700_internal_bus_reset(struct Scsi_Host *host)
{
	/* Bus reset */
	NCR_700_writeb(ASSERT_RST, host, SCNTL1_REG);
	udelay(50);
	NCR_700_writeb(0, host, SCNTL1_REG);

}

STATIC void
NCR_700_chip_setup(struct Scsi_Host *host)
{
	struct NCR_700_Host_Parameters *hostdata = 
		(struct NCR_700_Host_Parameters *)host->hostdata[0];
	__u32 dcntl_extra = 0;
	__u8 min_period;
	__u8 min_xferp = (hostdata->chip710 ? NCR_710_MIN_XFERP : NCR_700_MIN_XFERP);

	if(hostdata->chip710) {
		__u8 burst_disable = hostdata->burst_disable
			? BURST_DISABLE : 0;
		dcntl_extra = COMPAT_700_MODE;

		NCR_700_writeb(dcntl_extra, host, DCNTL_REG);
		NCR_700_writeb(BURST_LENGTH_8  | hostdata->dmode_extra,
			       host, DMODE_710_REG);
		NCR_700_writeb(burst_disable | (hostdata->differential ? 
						DIFF : 0), host, CTEST7_REG);
		NCR_700_writeb(BTB_TIMER_DISABLE, host, CTEST0_REG);
		NCR_700_writeb(FULL_ARBITRATION | ENABLE_PARITY | PARITY
			       | AUTO_ATN, host, SCNTL0_REG);
	} else {
		NCR_700_writeb(BURST_LENGTH_8 | hostdata->dmode_extra,
			       host, DMODE_700_REG);
		NCR_700_writeb(hostdata->differential ? 
			       DIFF : 0, host, CTEST7_REG);
		if(hostdata->fast) {
			/* this is for 700-66, does nothing on 700 */
			NCR_700_writeb(LAST_DIS_ENBL | ENABLE_ACTIVE_NEGATION 
				       | GENERATE_RECEIVE_PARITY, host,
				       CTEST8_REG);
		} else {
			NCR_700_writeb(FULL_ARBITRATION | ENABLE_PARITY
				       | PARITY | AUTO_ATN, host, SCNTL0_REG);
		}
	}

	NCR_700_writeb(1 << host->this_id, host, SCID_REG);
	NCR_700_writeb(0, host, SBCL_REG);
	NCR_700_writeb(ASYNC_OPERATION, host, SXFER_REG);

	NCR_700_writeb(PHASE_MM_INT | SEL_TIMEOUT_INT | GROSS_ERR_INT | UX_DISC_INT
	     | RST_INT | PAR_ERR_INT | SELECT_INT, host, SIEN_REG);

	NCR_700_writeb(ABORT_INT | INT_INST_INT | ILGL_INST_INT, host, DIEN_REG);
	NCR_700_writeb(ENABLE_SELECT, host, SCNTL1_REG);
	if(hostdata->clock > 75) {
		printk(KERN_ERR "53c700: Clock speed %dMHz is too high: 75Mhz is the maximum this chip can be driven at\n", hostdata->clock);
		/* do the best we can, but the async clock will be out
		 * of spec: sync divider 2, async divider 3 */
		DEBUG(("53c700: sync 2 async 3\n"));
		NCR_700_writeb(SYNC_DIV_2_0, host, SBCL_REG);
		NCR_700_writeb(ASYNC_DIV_3_0 | dcntl_extra, host, DCNTL_REG);
		hostdata->sync_clock = hostdata->clock/2;
	} else	if(hostdata->clock > 50  && hostdata->clock <= 75) {
		/* sync divider 1.5, async divider 3 */
		DEBUG(("53c700: sync 1.5 async 3\n"));
		NCR_700_writeb(SYNC_DIV_1_5, host, SBCL_REG);
		NCR_700_writeb(ASYNC_DIV_3_0 | dcntl_extra, host, DCNTL_REG);
		hostdata->sync_clock = hostdata->clock*2;
		hostdata->sync_clock /= 3;
		
	} else if(hostdata->clock > 37 && hostdata->clock <= 50) {
		/* sync divider 1, async divider 2 */
		DEBUG(("53c700: sync 1 async 2\n"));
		NCR_700_writeb(SYNC_DIV_1_0, host, SBCL_REG);
		NCR_700_writeb(ASYNC_DIV_2_0 | dcntl_extra, host, DCNTL_REG);
		hostdata->sync_clock = hostdata->clock;
	} else if(hostdata->clock > 25 && hostdata->clock <=37) {
		/* sync divider 1, async divider 1.5 */
		DEBUG(("53c700: sync 1 async 1.5\n"));
		NCR_700_writeb(SYNC_DIV_1_0, host, SBCL_REG);
		NCR_700_writeb(ASYNC_DIV_1_5 | dcntl_extra, host, DCNTL_REG);
		hostdata->sync_clock = hostdata->clock;
	} else {
		DEBUG(("53c700: sync 1 async 1\n"));
		NCR_700_writeb(SYNC_DIV_1_0, host, SBCL_REG);
		NCR_700_writeb(ASYNC_DIV_1_0 | dcntl_extra, host, DCNTL_REG);
		/* sync divider 1, async divider 1 */
		hostdata->sync_clock = hostdata->clock;
	}
	/* Calculate the actual minimum period that can be supported
	 * by our synchronous clock speed.  See the 710 manual for
	 * exact details of this calculation which is based on a
	 * setting of the SXFER register */
	min_period = 1000*(4+min_xferp)/(4*hostdata->sync_clock);
	if(min_period > NCR_700_MIN_PERIOD) {
		NCR_700_SDTR_msg[3] = min_period;
	}
	if(hostdata->chip710)
		NCR_700_SDTR_msg[4] = NCR_710_MAX_OFFSET;
}

STATIC void
NCR_700_chip_reset(struct Scsi_Host *host)
{
	struct NCR_700_Host_Parameters *hostdata = 
		(struct NCR_700_Host_Parameters *)host->hostdata[0];
	if(hostdata->chip710) {
		NCR_700_writeb(SOFTWARE_RESET_710, host, ISTAT_REG);
		udelay(100);

		NCR_700_writeb(0, host, ISTAT_REG);
	} else {
		NCR_700_writeb(SOFTWARE_RESET, host, DCNTL_REG);
		udelay(100);
		
		NCR_700_writeb(0, host, DCNTL_REG);
	}

	mdelay(1000);

	NCR_700_chip_setup(host);
}

/* The heart of the message processing engine is that the instruction
 * immediately after the INT is the normal case (and so must be CLEAR
 * ACK).  If we want to do something else, we call that routine in
 * scripts and set temp to be the normal case + 8 (skipping the CLEAR
 * ACK) so that the routine returns correctly to resume its activity
 * */
STATIC __u32
process_extended_message(struct Scsi_Host *host, 
			 struct NCR_700_Host_Parameters *hostdata,
			 Scsi_Cmnd *SCp, __u32 dsp, __u32 dsps)
{
	__u32 resume_offset = dsp, temp = dsp + 8;
	__u8 pun = 0xff, lun = 0xff;

	if(SCp != NULL) {
		pun = SCp->target;
		lun = SCp->lun;
	}

	switch(hostdata->msgin[2]) {
	case A_SDTR_MSG:
		if(SCp != NULL && NCR_700_is_flag_set(SCp->device, NCR_700_DEV_BEGIN_SYNC_NEGOTIATION)) {
			__u8 period = hostdata->msgin[3];
			__u8 offset = hostdata->msgin[4];
			__u8 sxfer;

			if(offset != 0 && period != 0)
				sxfer = NCR_700_offset_period_to_sxfer(hostdata, offset, period);
			else 
				sxfer = 0;
			
			if(sxfer != NCR_700_get_SXFER(SCp->device)) {
				printk(KERN_INFO "scsi%d: (%d:%d) Synchronous at offset %d, period %dns\n",
				       host->host_no, pun, lun,
				       offset, period*4);
				
				NCR_700_set_SXFER(SCp->device, sxfer);
			}
			

			NCR_700_set_flag(SCp->device, NCR_700_DEV_NEGOTIATED_SYNC);
			NCR_700_clear_flag(SCp->device, NCR_700_DEV_BEGIN_SYNC_NEGOTIATION);
			
			NCR_700_writeb(NCR_700_get_SXFER(SCp->device),
				       host, SXFER_REG);

		} else {
			/* SDTR message out of the blue, reject it */
			printk(KERN_WARNING "scsi%d Unexpected SDTR msg\n",
			       host->host_no);
			hostdata->msgout[0] = A_REJECT_MSG;
			NCR_700_dma_cache_wback((unsigned long)hostdata->msgout, 1);
			script_patch_16(hostdata->script, MessageCount, 1);
			/* SendMsgOut returns, so set up the return
			 * address */
			resume_offset = hostdata->pScript + Ent_SendMessageWithATN;
		}
		break;
	
	case A_WDTR_MSG:
		printk(KERN_INFO "scsi%d: (%d:%d), Unsolicited WDTR after CMD, Rejecting\n",
		       host->host_no, pun, lun);
		hostdata->msgout[0] = A_REJECT_MSG;
		NCR_700_dma_cache_wback((unsigned long)hostdata->msgout, 1);
		script_patch_16(hostdata->script, MessageCount, 1);
		resume_offset = hostdata->pScript + Ent_SendMessageWithATN;

		break;

	default:
		printk(KERN_INFO "scsi%d (%d:%d): Unexpected message %s: ",
		       host->host_no, pun, lun,
		       NCR_700_phase[(dsps & 0xf00) >> 8]);
		print_msg(hostdata->msgin);
		printk("\n");
		/* just reject it */
		hostdata->msgout[0] = A_REJECT_MSG;
		NCR_700_dma_cache_wback((unsigned long)hostdata->msgout, 1);
		script_patch_16(hostdata->script, MessageCount, 1);
		/* SendMsgOut returns, so set up the return
		 * address */
		resume_offset = hostdata->pScript + Ent_SendMessageWithATN;
	}
	NCR_700_writel(temp, host, TEMP_REG);
	return resume_offset;
}

STATIC __u32
process_message(struct Scsi_Host *host,	struct NCR_700_Host_Parameters *hostdata,
		Scsi_Cmnd *SCp, __u32 dsp, __u32 dsps)
{
	/* work out where to return to */
	__u32 temp = dsp + 8, resume_offset = dsp;
	__u8 pun = 0xff, lun = 0xff;

	if(SCp != NULL) {
		pun = SCp->target;
		lun = SCp->lun;
	}

#ifdef NCR_700_DEBUG
	printk("scsi%d (%d:%d): message %s: ", host->host_no, pun, lun,
	       NCR_700_phase[(dsps & 0xf00) >> 8]);
	print_msg(hostdata->msgin);
	printk("\n");
#endif

	switch(hostdata->msgin[0]) {

	case A_EXTENDED_MSG:
		resume_offset =  process_extended_message(host, hostdata, SCp,
							  dsp, dsps);
		break;

	case A_REJECT_MSG:
		if(SCp != NULL && NCR_700_is_flag_set(SCp->device, NCR_700_DEV_BEGIN_SYNC_NEGOTIATION)) {
			/* Rejected our sync negotiation attempt */
			NCR_700_set_SXFER(SCp->device, 0);
			NCR_700_set_flag(SCp->device, NCR_700_DEV_NEGOTIATED_SYNC);
			NCR_700_clear_flag(SCp->device, NCR_700_DEV_BEGIN_SYNC_NEGOTIATION);
		} else if(SCp != NULL && NCR_700_is_flag_set(SCp->device, NCR_700_DEV_BEGIN_TAG_QUEUEING)) {
			/* rejected our first simple tag message */
			printk(KERN_WARNING "scsi%d (%d:%d) Rejected first tag queue attempt, turning off tag queueing\n", host->host_no, pun, lun);
			NCR_700_clear_flag(SCp->device, NCR_700_DEV_BEGIN_TAG_QUEUEING);
			hostdata->tag_negotiated &= ~(1<<SCp->target);
			SCp->device->tagged_queue = 0;
			SCp->device->tagged_supported = 0;
		} else {
			printk(KERN_WARNING "scsi%d (%d:%d) Unexpected REJECT Message %s\n",
			       host->host_no, pun, lun,
			       NCR_700_phase[(dsps & 0xf00) >> 8]);
			/* however, just ignore it */
		}
		break;

	case A_PARITY_ERROR_MSG:
		printk(KERN_ERR "scsi%d (%d:%d) Parity Error!\n", host->host_no,
		       pun, lun);
		NCR_700_internal_bus_reset(host);
		break;
	case A_SIMPLE_TAG_MSG:
		printk(KERN_INFO "scsi%d (%d:%d) SIMPLE TAG %d %s\n", host->host_no,
		       pun, lun, hostdata->msgin[1],
		       NCR_700_phase[(dsps & 0xf00) >> 8]);
		/* just ignore it */
		break;
	default:
		printk(KERN_INFO "scsi%d (%d:%d): Unexpected message %s: ",
		       host->host_no, pun, lun,
		       NCR_700_phase[(dsps & 0xf00) >> 8]);

		print_msg(hostdata->msgin);
		printk("\n");
		/* just reject it */
		hostdata->msgout[0] = A_REJECT_MSG;
		NCR_700_dma_cache_wback((unsigned long)hostdata->msgout, 1);
		script_patch_16(hostdata->script, MessageCount, 1);
		/* SendMsgOut returns, so set up the return
		 * address */
		resume_offset = hostdata->pScript + Ent_SendMessageWithATN;

		break;
	}
	NCR_700_writel(temp, host, TEMP_REG);
	/* set us up to receive another message */
	NCR_700_dma_cache_inv((unsigned long)hostdata->msgin, MSG_ARRAY_SIZE);
	return resume_offset;
}

STATIC __u32
process_script_interrupt(__u32 dsps, __u32 dsp, Scsi_Cmnd *SCp,
			 struct Scsi_Host *host,
			 struct NCR_700_Host_Parameters *hostdata)
{
	__u32 resume_offset = 0;
	__u8 pun = 0xff, lun=0xff;

	if(SCp != NULL) {
		pun = SCp->target;
		lun = SCp->lun;
	}

	if(dsps == A_GOOD_STATUS_AFTER_STATUS) {
		DEBUG(("  COMMAND COMPLETE, status=%02x\n",
		       hostdata->status[0]));
		/* OK, if TCQ still on, we know it works */
		NCR_700_clear_flag(SCp->device, NCR_700_DEV_BEGIN_TAG_QUEUEING);
		/* check for contingent allegiance contitions */
		if(status_byte(hostdata->status[0]) == CHECK_CONDITION ||
		   status_byte(hostdata->status[0]) == COMMAND_TERMINATED) {
			struct NCR_700_command_slot *slot =
				(struct NCR_700_command_slot *)SCp->host_scribble;
			if(SCp->cmnd[0] == REQUEST_SENSE) {
				/* OOPS: bad device, returning another
				 * contingent allegiance condition */
				printk(KERN_ERR "scsi%d (%d:%d) broken device is looping in contingent allegiance: ignoring\n", host->host_no, pun, lun);
				NCR_700_scsi_done(hostdata, SCp, hostdata->status[0]);
			} else {
#ifdef NCR_DEBUG
				print_command(SCp->cmnd);
				printk("  cmd %p has status %d, requesting sense\n",
				       SCp, hostdata->status[0]);
#endif
				/* we can destroy the command here
				 * because the contingent allegiance
				 * condition will cause a retry which
				 * will re-copy the command from the
				 * saved data_cmnd.  We also unmap any
				 * data associated with the command
				 * here */
				NCR_700_unmap(hostdata, SCp, slot);

				SCp->cmnd[0] = REQUEST_SENSE;
				SCp->cmnd[1] = (SCp->lun & 0x7) << 5;
				SCp->cmnd[2] = 0;
				SCp->cmnd[3] = 0;
				SCp->cmnd[4] = sizeof(SCp->sense_buffer);
				SCp->cmnd[5] = 0;
				SCp->cmd_len = 6;
				/* Here's a quiet hack: the
				 * REQUEST_SENSE command is six bytes,
				 * so store a flag indicating that
				 * this was an internal sense request
				 * and the original status at the end
				 * of the command */
				SCp->cmnd[6] = NCR_700_INTERNAL_SENSE_MAGIC;
				SCp->cmnd[7] = hostdata->status[0];
				SCp->use_sg = 0;
				SCp->sc_data_direction = SCSI_DATA_READ;
				pci_dma_sync_single(hostdata->pci_dev,
						    slot->pCmd,
						    SCp->cmd_len,
						    PCI_DMA_TODEVICE);
				SCp->request_bufflen = sizeof(SCp->sense_buffer);
				slot->dma_handle = pci_map_single(hostdata->pci_dev, SCp->sense_buffer, sizeof(SCp->sense_buffer), PCI_DMA_FROMDEVICE);
				slot->SG[0].ins = bS_to_host(SCRIPT_MOVE_DATA_IN | sizeof(SCp->sense_buffer));
				slot->SG[0].pAddr = bS_to_host(slot->dma_handle);
				slot->SG[1].ins = bS_to_host(SCRIPT_RETURN);
				slot->SG[1].pAddr = 0;
				slot->resume_offset = hostdata->pScript;
				NCR_700_dma_cache_wback((unsigned long)slot->SG, sizeof(slot->SG[0])*2);
				NCR_700_dma_cache_inv((unsigned long)SCp->sense_buffer, sizeof(SCp->sense_buffer));
				
				/* queue the command for reissue */
				slot->state = NCR_700_SLOT_QUEUED;
				hostdata->state = NCR_700_HOST_FREE;
				hostdata->cmd = NULL;
			}
		} else {
			// Currently rely on the mid layer evaluation
			// of the tag queuing capability
			//
			//if(status_byte(hostdata->status[0]) == GOOD &&
			//   SCp->cmnd[0] == INQUIRY && SCp->use_sg == 0) {
			//	/* Piggy back the tag queueing support
			//	 * on this command */
			//	pci_dma_sync_single(hostdata->pci_dev,
			//			    slot->dma_handle,
			//			    SCp->request_bufflen,
			//			    PCI_DMA_FROMDEVICE);
			//	if(((char *)SCp->request_buffer)[7] & 0x02) {
			//		printk(KERN_INFO "scsi%d: (%d:%d) Enabling Tag Command Queuing\n", host->host_no, pun, lun);
			//		hostdata->tag_negotiated |= (1<<SCp->target);
			//		NCR_700_set_flag(SCp->device, NCR_700_DEV_BEGIN_TAG_QUEUEING);
			//	} else {
			//		NCR_700_clear_flag(SCp->device, NCR_700_DEV_BEGIN_TAG_QUEUEING);
			//		hostdata->tag_negotiated &= ~(1<<SCp->target);
			//	}
			//}
			NCR_700_scsi_done(hostdata, SCp, hostdata->status[0]);
		}
	} else if((dsps & 0xfffff0f0) == A_UNEXPECTED_PHASE) {
		__u8 i = (dsps & 0xf00) >> 8;

		printk(KERN_ERR "scsi%d: (%d:%d), UNEXPECTED PHASE %s (%s)\n",
		       host->host_no, pun, lun,
		       NCR_700_phase[i],
		       sbcl_to_string(NCR_700_readb(host, SBCL_REG)));
		printk(KERN_ERR "         len = %d, cmd =", SCp->cmd_len);
		print_command(SCp->cmnd);

		NCR_700_internal_bus_reset(host);
	} else if((dsps & 0xfffff000) == A_FATAL) {
		int i = (dsps & 0xfff);

		printk(KERN_ERR "scsi%d: (%d:%d) FATAL ERROR: %s\n",
		       host->host_no, pun, lun, NCR_700_fatal_messages[i]);
		if(dsps == A_FATAL_ILLEGAL_MSG_LENGTH) {
			printk(KERN_ERR "     msg begins %02x %02x\n",
			       hostdata->msgin[0], hostdata->msgin[1]);
		}
		NCR_700_internal_bus_reset(host);
	} else if((dsps & 0xfffff0f0) == A_DISCONNECT) {
#ifdef NCR_700_DEBUG
		__u8 i = (dsps & 0xf00) >> 8;

		printk("scsi%d: (%d:%d), DISCONNECTED (%d) %s\n",
		       host->host_no, pun, lun,
		       i, NCR_700_phase[i]);
#endif
		save_for_reselection(hostdata, SCp, dsp);

	} else if(dsps == A_RESELECTION_IDENTIFIED) {
		__u8 lun;
		struct NCR_700_command_slot *slot;
		__u8 reselection_id = hostdata->reselection_id;

		lun = hostdata->msgin[0] & 0x1f;

		hostdata->reselection_id = 0xff;
		DEBUG(("scsi%d: (%d:%d) RESELECTED!\n",
		       host->host_no, reselection_id, lun));
		/* clear the reselection indicator */
		if(hostdata->msgin[1] == A_SIMPLE_TAG_MSG) {
			slot = find_ITLQ_Nexus(hostdata, reselection_id,
					       lun, hostdata->msgin[2]);
		} else {
			slot = find_ITL_Nexus(hostdata, reselection_id, lun);
		}
	retry:
		if(slot == NULL) {
			struct NCR_700_command_slot *s = find_ITL_Nexus(hostdata, reselection_id, lun);
			printk(KERN_ERR "scsi%d: (%d:%d) RESELECTED but no saved command (MSG = %02x %02x %02x)!!\n",
			       host->host_no, reselection_id, lun,
			       hostdata->msgin[0], hostdata->msgin[1],
			       hostdata->msgin[2]);
			printk(KERN_ERR " OUTSTANDING TAGS:");
			while(s != NULL) {
				if(s->cmnd->target == reselection_id &&
				   s->cmnd->lun == lun) {
					printk("%d ", s->tag);
					if(s->tag == hostdata->msgin[2]) {
						printk(" ***FOUND*** \n");
						slot = s;
						goto retry;
					}
						
				}
				s = s->ITL_back;
			}
			printk("\n");
		} else {
			if(hostdata->state != NCR_700_HOST_BUSY)
				printk(KERN_ERR "scsi%d: FATAL, host not busy during valid reselection!\n",
				       host->host_no);
			resume_offset = slot->resume_offset;
			hostdata->cmd = slot->cmnd;

			/* re-patch for this command */
			script_patch_32_abs(hostdata->script, CommandAddress, 
					    slot->pCmd);
			script_patch_16(hostdata->script,
					CommandCount, slot->cmnd->cmd_len);
			script_patch_32_abs(hostdata->script, SGScriptStartAddress,
					    to32bit(&slot->pSG[0].ins));

			/* Note: setting SXFER only works if we're
			 * still in the MESSAGE phase, so it is vital
			 * that ACK is still asserted when we process
			 * the reselection message.  The resume offset
			 * should therefore always clear ACK */
			NCR_700_writeb(NCR_700_get_SXFER(hostdata->cmd->device),
				       host, SXFER_REG);
			NCR_700_dma_cache_inv((unsigned long)hostdata->msgin,
				      MSG_ARRAY_SIZE);
			NCR_700_dma_cache_wback((unsigned long)hostdata->msgout,
					MSG_ARRAY_SIZE);
			/* I'm just being paranoid here, the command should
			 * already have been flushed from the cache */
			NCR_700_dma_cache_wback((unsigned long)slot->cmnd->cmnd,
					slot->cmnd->cmd_len);


			
		}
	} else if(dsps == A_RESELECTED_DURING_SELECTION) {

		/* This section is full of debugging code because I've
		 * never managed to reach it.  I think what happens is
		 * that, because the 700 runs with selection
		 * interrupts enabled the whole time that we take a
		 * selection interrupt before we manage to get to the
		 * reselected script interrupt */

		__u8 reselection_id = NCR_700_readb(host, SFBR_REG);
		struct NCR_700_command_slot *slot;
		
		/* Take out our own ID */
		reselection_id &= ~(1<<host->this_id);
		
		/* I've never seen this happen, so keep this as a printk rather
		 * than a debug */
		printk(KERN_INFO "scsi%d: (%d:%d) RESELECTION DURING SELECTION, dsp=%08x[%04x] state=%d, count=%d\n",
		       host->host_no, reselection_id, lun, dsp, dsp - hostdata->pScript, hostdata->state, hostdata->command_slot_count);

		{
			/* FIXME: DEBUGGING CODE */
			__u32 SG = (__u32)bS_to_cpu(hostdata->script[A_SGScriptStartAddress_used[0]]);
			int i;

			for(i=0; i< NCR_700_COMMAND_SLOTS_PER_HOST; i++) {
				if(SG >= to32bit(&hostdata->slots[i].pSG[0])
				   && SG <= to32bit(&hostdata->slots[i].pSG[NCR_700_SG_SEGMENTS]))
					break;
			}
			printk(KERN_INFO "IDENTIFIED SG segment as being %08x in slot %p, cmd %p, slot->resume_offset=%08x\n", SG, &hostdata->slots[i], hostdata->slots[i].cmnd, hostdata->slots[i].resume_offset);
			SCp =  hostdata->slots[i].cmnd;
		}

		if(SCp != NULL) {
			slot = (struct NCR_700_command_slot *)SCp->host_scribble;
			/* change slot from busy to queued to redo command */
			slot->state = NCR_700_SLOT_QUEUED;
		}
		hostdata->cmd = NULL;
		
		if(reselection_id == 0) {
			if(hostdata->reselection_id == 0xff) {
				printk(KERN_ERR "scsi%d: Invalid reselection during selection!!\n", host->host_no);
				return 0;
			} else {
				printk(KERN_ERR "scsi%d: script reselected and we took a selection interrupt\n",
				       host->host_no);
				reselection_id = hostdata->reselection_id;
			}
		} else {
			
			/* convert to real ID */
			reselection_id = bitmap_to_number(reselection_id);
		}
		hostdata->reselection_id = reselection_id;
		/* just in case we have a stale simple tag message, clear it */
		hostdata->msgin[1] = 0;
		NCR_700_dma_cache_wback_inv((unsigned long)hostdata->msgin,
					    MSG_ARRAY_SIZE);
		if(hostdata->tag_negotiated & (1<<reselection_id)) {
			resume_offset = hostdata->pScript + Ent_GetReselectionWithTag;
		} else {
			resume_offset = hostdata->pScript + Ent_GetReselectionData;
		}
	} else if(dsps == A_COMPLETED_SELECTION_AS_TARGET) {
		/* we've just disconnected from the bus, do nothing since
		 * a return here will re-run the queued command slot
		 * that may have been interrupted by the initial selection */
		DEBUG((" SELECTION COMPLETED\n"));
	} else if((dsps & 0xfffff0f0) == A_MSG_IN) { 
		resume_offset = process_message(host, hostdata, SCp,
						dsp, dsps);
	} else if((dsps &  0xfffff000) == 0) {
		__u8 i = (dsps & 0xf0) >> 4, j = (dsps & 0xf00) >> 8;
		printk(KERN_ERR "scsi%d: (%d:%d), unhandled script condition %s %s at %04x\n",
		       host->host_no, pun, lun, NCR_700_condition[i],
		       NCR_700_phase[j], dsp - hostdata->pScript);
		if(SCp != NULL) {
			print_command(SCp->cmnd);

			if(SCp->use_sg) {
				for(i = 0; i < SCp->use_sg + 1; i++) {
					printk(KERN_INFO " SG[%d].length = %d, move_insn=%08x, addr %08x\n", i, ((struct scatterlist *)SCp->buffer)[i].length, ((struct NCR_700_command_slot *)SCp->host_scribble)->SG[i].ins, ((struct NCR_700_command_slot *)SCp->host_scribble)->SG[i].pAddr);
				}
			}
		}	       
		NCR_700_internal_bus_reset(host);
	} else if((dsps & 0xfffff000) == A_DEBUG_INTERRUPT) {
		printk(KERN_NOTICE "scsi%d (%d:%d) DEBUG INTERRUPT %d AT %08x[%04x], continuing\n",
		       host->host_no, pun, lun, dsps & 0xfff, dsp, dsp - hostdata->pScript);
		resume_offset = dsp;
	} else {
		printk(KERN_ERR "scsi%d: (%d:%d), unidentified script interrupt 0x%x at %04x\n",
		       host->host_no, pun, lun, dsps, dsp - hostdata->pScript);
		NCR_700_internal_bus_reset(host);
	}
	return resume_offset;
}

/* We run the 53c700 with selection interrupts always enabled.  This
 * means that the chip may be selected as soon as the bus frees.  On a
 * busy bus, this can be before the scripts engine finishes its
 * processing.  Therefore, part of the selection processing has to be
 * to find out what the scripts engine is doing and complete the
 * function if necessary (i.e. process the pending disconnect or save
 * the interrupted initial selection */
STATIC inline __u32
process_selection(struct Scsi_Host *host, __u32 dsp)
{
	__u8 id = 0;	/* Squash compiler warning */
	int count = 0;
	__u32 resume_offset = 0;
	struct NCR_700_Host_Parameters *hostdata =
		(struct NCR_700_Host_Parameters *)host->hostdata[0];
	Scsi_Cmnd *SCp = hostdata->cmd;
	__u8 sbcl;

	for(count = 0; count < 5; count++) {
		id = NCR_700_readb(host, hostdata->chip710 ?
				   CTEST9_REG : SFBR_REG);

		/* Take out our own ID */
		id &= ~(1<<host->this_id);
		if(id != 0) 
			break;
		udelay(5);
	}
	sbcl = NCR_700_readb(host, SBCL_REG);
	if((sbcl & SBCL_IO) == 0) {
		/* mark as having been selected rather than reselected */
		id = 0xff;
	} else {
		/* convert to real ID */
		hostdata->reselection_id = id = bitmap_to_number(id);
		DEBUG(("scsi%d:  Reselected by %d\n",
		       host->host_no, id));
	}
	if(hostdata->state == NCR_700_HOST_BUSY && SCp != NULL) {
		struct NCR_700_command_slot *slot =
			(struct NCR_700_command_slot *)SCp->host_scribble;
		DEBUG(("  ID %d WARNING: RESELECTION OF BUSY HOST, saving cmd %p, slot %p, addr %x [%04x], resume %x!\n", id, hostdata->cmd, slot, dsp, dsp - hostdata->pScript, resume_offset));
		
		switch(dsp - hostdata->pScript) {
		case Ent_Disconnect1:
		case Ent_Disconnect2:
			save_for_reselection(hostdata, SCp, Ent_Disconnect2 + hostdata->pScript);
			break;
		case Ent_Disconnect3:
		case Ent_Disconnect4:
			save_for_reselection(hostdata, SCp, Ent_Disconnect4 + hostdata->pScript);
			break;
		case Ent_Disconnect5:
		case Ent_Disconnect6:
			save_for_reselection(hostdata, SCp, Ent_Disconnect6 + hostdata->pScript);
			break;
		case Ent_Disconnect7:
		case Ent_Disconnect8:
			save_for_reselection(hostdata, SCp, Ent_Disconnect8 + hostdata->pScript);
			break;
		case Ent_Finish1:
		case Ent_Finish2:
			process_script_interrupt(A_GOOD_STATUS_AFTER_STATUS, dsp, SCp, host, hostdata);
			break;
			
		default:
			slot->state = NCR_700_SLOT_QUEUED;
			break;
			}
	}
	hostdata->state = NCR_700_HOST_BUSY;
	hostdata->cmd = NULL;
	/* clear any stale simple tag message */
	hostdata->msgin[1] = 0;
	NCR_700_dma_cache_wback_inv((unsigned long)hostdata->msgin, MSG_ARRAY_SIZE);

	if(id == 0xff) {
		/* Selected as target, Ignore */
		resume_offset = hostdata->pScript + Ent_SelectedAsTarget;
	} else if(hostdata->tag_negotiated & (1<<id)) {
		resume_offset = hostdata->pScript + Ent_GetReselectionWithTag;
	} else {
		resume_offset = hostdata->pScript + Ent_GetReselectionData;
	}
	return resume_offset;
}

static inline void
NCR_700_clear_fifo(struct Scsi_Host *host) {
	const struct NCR_700_Host_Parameters *hostdata
		= (struct NCR_700_Host_Parameters *)host->hostdata[0];
	if(hostdata->chip710) {
		NCR_700_writeb(CLR_FIFO_710, host, CTEST8_REG);
	} else {
		NCR_700_writeb(CLR_FIFO, host, DFIFO_REG);
	}
}

static inline void
NCR_700_flush_fifo(struct Scsi_Host *host) {
	const struct NCR_700_Host_Parameters *hostdata
		= (struct NCR_700_Host_Parameters *)host->hostdata[0];
	if(hostdata->chip710) {
		NCR_700_writeb(FLUSH_DMA_FIFO_710, host, CTEST8_REG);
		udelay(10);
		NCR_700_writeb(0, host, CTEST8_REG);
	} else {
		NCR_700_writeb(FLUSH_DMA_FIFO, host, DFIFO_REG);
		udelay(10);
		NCR_700_writeb(0, host, DFIFO_REG);
	}
}


STATIC int
NCR_700_start_command(Scsi_Cmnd *SCp)
{
	struct NCR_700_command_slot *slot =
		(struct NCR_700_command_slot *)SCp->host_scribble;
	struct NCR_700_Host_Parameters *hostdata =
		(struct NCR_700_Host_Parameters *)SCp->host->hostdata[0];
	unsigned long flags;
	__u16 count = 1;	/* for IDENTIFY message */
	
	save_flags(flags);
	cli();
	if(hostdata->state != NCR_700_HOST_FREE) {
		/* keep this inside the lock to close the race window where
		 * the running command finishes on another CPU while we don't
		 * change the state to queued on this one */
		slot->state = NCR_700_SLOT_QUEUED;
		restore_flags(flags);

		DEBUG(("scsi%d: host busy, queueing command %p, slot %p\n",
		       SCp->host->host_no, slot->cmnd, slot));
		return 0;
	}
	hostdata->state = NCR_700_HOST_BUSY;
	hostdata->cmd = SCp;
	slot->state = NCR_700_SLOT_BUSY;
	/* keep interrupts disabled until we have the command correctly
	 * set up so we cannot take a selection interrupt */

	hostdata->msgout[0] = NCR_700_identify(SCp->cmnd[0] != REQUEST_SENSE,
					       SCp->lun);
	/* for INQUIRY or REQUEST_SENSE commands, we cannot be sure
	 * if the negotiated transfer parameters still hold, so
	 * always renegotiate them */
	if(SCp->cmnd[0] == INQUIRY || SCp->cmnd[0] == REQUEST_SENSE) {
		NCR_700_clear_flag(SCp->device, NCR_700_DEV_NEGOTIATED_SYNC);
	}

	/* REQUEST_SENSE is asking for contingent I_T_L(_Q) status.
	 * If a contingent allegiance condition exists, the device
	 * will refuse all tags, so send the request sense as untagged
	 * */
	if((hostdata->tag_negotiated & (1<<SCp->target))
	   && (slot->tag != NCR_700_NO_TAG && SCp->cmnd[0] != REQUEST_SENSE)) {
		hostdata->msgout[count++] = A_SIMPLE_TAG_MSG;
		hostdata->msgout[count++] = slot->tag;
	}

	if(hostdata->fast &&
	   NCR_700_is_flag_clear(SCp->device, NCR_700_DEV_NEGOTIATED_SYNC)) {
		memcpy(&hostdata->msgout[count], NCR_700_SDTR_msg,
		       sizeof(NCR_700_SDTR_msg));
		count += sizeof(NCR_700_SDTR_msg);
		NCR_700_set_flag(SCp->device, NCR_700_DEV_BEGIN_SYNC_NEGOTIATION);
	}

	script_patch_16(hostdata->script, MessageCount, count);


	script_patch_ID(hostdata->script,
			Device_ID, 1<<SCp->target);

	script_patch_32_abs(hostdata->script, CommandAddress, 
			    slot->pCmd);
	script_patch_16(hostdata->script, CommandCount, SCp->cmd_len);
	/* finally plumb the beginning of the SG list into the script
	 * */
	script_patch_32_abs(hostdata->script, SGScriptStartAddress,
			    to32bit(&slot->pSG[0].ins));
	NCR_700_clear_fifo(SCp->host);

	if(slot->resume_offset == 0)
		slot->resume_offset = hostdata->pScript;
	/* now perform all the writebacks and invalidates */
	NCR_700_dma_cache_wback((unsigned long)hostdata->msgout, count);
	NCR_700_dma_cache_inv((unsigned long)hostdata->msgin, MSG_ARRAY_SIZE);
	NCR_700_dma_cache_wback((unsigned long)SCp->cmnd, SCp->cmd_len);
	NCR_700_dma_cache_inv((unsigned long)hostdata->status, 1);

	/* set the synchronous period/offset */
	NCR_700_writeb(NCR_700_get_SXFER(SCp->device),
		       SCp->host, SXFER_REG);
	NCR_700_writel(slot->temp, SCp->host, TEMP_REG);
	NCR_700_writel(slot->resume_offset, SCp->host, DSP_REG);

	/* allow interrupts here so that if we're selected we can take
	 * a selection interrupt.  The script start may not be
	 * effective in this case, but the selection interrupt will
	 * save our command in that case */
	restore_flags(flags);

	return 1;
}

void
NCR_700_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	struct Scsi_Host *host = (struct Scsi_Host *)dev_id;
	struct NCR_700_Host_Parameters *hostdata =
		(struct NCR_700_Host_Parameters *)host->hostdata[0];
	__u8 istat;
	__u32 resume_offset = 0;
	__u8 pun = 0xff, lun = 0xff;
	unsigned long flags;

	/* Unfortunately, we have to take the io_request_lock here
	 * rather than the host lock hostdata->lock because we're
	 * looking to exclude queuecommand from messing with the
	 * registers while we're processing the interrupt.  Since
	 * queuecommand is called holding io_request_lock, and we have
	 * to take io_request_lock before we call the command
	 * scsi_done, we would get a deadlock if we took
	 * hostdata->lock here and in queuecommand (because the order
	 * of locking in queuecommand: 1) io_request_lock then 2)
	 * hostdata->lock would be the reverse of taking it in this
	 * routine */
	spin_lock_irqsave(&io_request_lock, flags);
	if((istat = NCR_700_readb(host, ISTAT_REG))
	      & (SCSI_INT_PENDING | DMA_INT_PENDING)) {
		__u32 dsps;
		__u8 sstat0 = 0, dstat = 0;
		__u32 dsp;
		Scsi_Cmnd *SCp = hostdata->cmd;
		enum NCR_700_Host_State state;

		state = hostdata->state;
		SCp = hostdata->cmd;

		if(istat & SCSI_INT_PENDING) {
			udelay(10);

			sstat0 = NCR_700_readb(host, SSTAT0_REG);
		}

		if(istat & DMA_INT_PENDING) {
			udelay(10);

			dstat = NCR_700_readb(host, DSTAT_REG);
		}

		dsps = NCR_700_readl(host, DSPS_REG);
		dsp = NCR_700_readl(host, DSP_REG);

		DEBUG(("scsi%d: istat %02x sstat0 %02x dstat %02x dsp %04x[%08x] dsps 0x%x\n",
		       host->host_no, istat, sstat0, dstat,
		       (dsp - (__u32)(hostdata->pScript))/4,
		       dsp, dsps));

		if(SCp != NULL) {
			pun = SCp->target;
			lun = SCp->lun;
		}

		if(sstat0 & SCSI_RESET_DETECTED) {
			Scsi_Device *SDp;
			int i;

			hostdata->state = NCR_700_HOST_BUSY;

			printk(KERN_ERR "scsi%d: Bus Reset detected, executing command %p, slot %p, dsp %08x[%04x]\n",
			       host->host_no, SCp, SCp == NULL ? NULL : SCp->host_scribble, dsp, dsp - hostdata->pScript);

			/* clear all the negotiated parameters */
			for(SDp = host->host_queue; SDp != NULL; SDp = SDp->next)
				SDp->hostdata = 0;
			
			/* clear all the slots and their pending commands */
			for(i = 0; i < NCR_700_COMMAND_SLOTS_PER_HOST; i++) {
				Scsi_Cmnd *SCp;
				struct NCR_700_command_slot *slot =
					&hostdata->slots[i];

				if(slot->state == NCR_700_SLOT_FREE)
					continue;
				
				SCp = slot->cmnd;
				printk(KERN_ERR " failing command because of reset, slot %p, cmnd %p\n",
				       slot, SCp);
				free_slot(slot, hostdata);
				SCp->host_scribble = NULL;
				NCR_700_set_depth(SCp->device, 0);
				/* NOTE: deadlock potential here: we
				 * rely on mid-layer guarantees that
				 * scsi_done won't try to issue the
				 * command again otherwise we'll
				 * deadlock on the
				 * hostdata->state_lock */
				SCp->result = DID_RESET << 16;
				SCp->scsi_done(SCp);
			}
			mdelay(25);
			NCR_700_chip_setup(host);

			hostdata->state = NCR_700_HOST_FREE;
			hostdata->cmd = NULL;
			goto out_unlock;
		} else if(sstat0 & SELECTION_TIMEOUT) {
			DEBUG(("scsi%d: (%d:%d) selection timeout\n",
			       host->host_no, pun, lun));
			NCR_700_scsi_done(hostdata, SCp, DID_NO_CONNECT<<16);
		} else if(sstat0 & PHASE_MISMATCH) {
			struct NCR_700_command_slot *slot = (SCp == NULL) ? NULL :
				(struct NCR_700_command_slot *)SCp->host_scribble;

			if(dsp == Ent_SendMessage + 8 + hostdata->pScript) {
				/* It wants to reply to some part of
				 * our message */
#ifdef NCR_700_DEBUG
				__u32 temp = NCR_700_readl(host, TEMP_REG);
				int count = (hostdata->script[Ent_SendMessage/4] & 0xffffff) - ((NCR_700_readl(host, DBC_REG) & 0xffffff) + NCR_700_data_residual(host));
				printk("scsi%d (%d:%d) PHASE MISMATCH IN SEND MESSAGE %d remain, return %p[%04x], phase %s\n", host->host_no, pun, lun, count, (void *)temp, temp - hostdata->pScript, sbcl_to_string(NCR_700_readb(host, SBCL_REG)));
#endif
				resume_offset = hostdata->pScript + Ent_SendMessagePhaseMismatch;
			} else if(dsp >= to32bit(&slot->pSG[0].ins) &&
				  dsp <= to32bit(&slot->pSG[NCR_700_SG_SEGMENTS].ins)) {
				int data_transfer = NCR_700_readl(host, DBC_REG) & 0xffffff;
				int SGcount = (dsp - to32bit(&slot->pSG[0].ins))/sizeof(struct NCR_700_SG_List);
				int residual = NCR_700_data_residual(host);
				int i;
#ifdef NCR_700_DEBUG
				__u32 naddr = NCR_700_readl(host, DNAD_REG);

				printk("scsi%d: (%d:%d) Expected phase mismatch in slot->SG[%d], transferred 0x%x\n",
				       host->host_no, pun, lun,
				       SGcount, data_transfer);
				print_command(SCp->cmnd);
				if(residual) {
					printk("scsi%d: (%d:%d) Expected phase mismatch in slot->SG[%d], transferred 0x%x, residual %d\n",
				       host->host_no, pun, lun,
				       SGcount, data_transfer, residual);
				}
#endif
				data_transfer += residual;

				if(data_transfer != 0) {
					int count; 
					__u32 pAddr;

					SGcount--;

					count = (bS_to_cpu(slot->SG[SGcount].ins) & 0x00ffffff);
					DEBUG(("DATA TRANSFER MISMATCH, count = %d, transferred %d\n", count, count-data_transfer));
					slot->SG[SGcount].ins &= bS_to_host(0xff000000);
					slot->SG[SGcount].ins |= bS_to_host(data_transfer);
					pAddr = bS_to_cpu(slot->SG[SGcount].pAddr);
					pAddr += (count - data_transfer);
#ifdef NCR_700_DEBUG
					if(pAddr != naddr) {
						printk("scsi%d (%d:%d) transfer mismatch pAddr=%lx, naddr=%lx, data_transfer=%d, residual=%d\n", host->host_no, pun, lun, (unsigned long)pAddr, (unsigned long)naddr, data_transfer, residual);
					}
#endif
					slot->SG[SGcount].pAddr = bS_to_host(pAddr);
				}
				/* set the executed moves to nops */
				for(i=0; i<SGcount; i++) {
					slot->SG[i].ins = bS_to_host(SCRIPT_NOP);
					slot->SG[i].pAddr = 0;
				}
				NCR_700_dma_cache_wback((unsigned long)slot->SG, sizeof(slot->SG));
				/* and pretend we disconnected after
				 * the command phase */
				resume_offset = hostdata->pScript + Ent_MsgInDuringData;
				/* make sure all the data is flushed */
				NCR_700_flush_fifo(host);
			} else {
				__u8 sbcl = NCR_700_readb(host, SBCL_REG);
				printk(KERN_ERR "scsi%d: (%d:%d) phase mismatch at %04x, phase %s\n",
				       host->host_no, pun, lun, dsp - hostdata->pScript, sbcl_to_string(sbcl));
				NCR_700_internal_bus_reset(host);
			}

		} else if(sstat0 & SCSI_GROSS_ERROR) {
			printk(KERN_ERR "scsi%d: (%d:%d) GROSS ERROR\n",
			       host->host_no, pun, lun);
			NCR_700_scsi_done(hostdata, SCp, DID_ERROR<<16);
		} else if(sstat0 & PARITY_ERROR) {
			printk(KERN_ERR "scsi%d: (%d:%d) PARITY ERROR\n",
			       host->host_no, pun, lun);
			NCR_700_scsi_done(hostdata, SCp, DID_ERROR<<16);
		} else if(dstat & SCRIPT_INT_RECEIVED) {
			DEBUG(("scsi%d: (%d:%d) ====>SCRIPT INTERRUPT<====\n",
			       host->host_no, pun, lun));
			resume_offset = process_script_interrupt(dsps, dsp, SCp, host, hostdata);
		} else if(dstat & (ILGL_INST_DETECTED)) {
			printk(KERN_ERR "scsi%d: (%d:%d) Illegal Instruction detected at 0x%08x[0x%x]!!!\n"
			       "         Please email James.Bottomley@HansenPartnership.com with the details\n",
			       host->host_no, pun, lun,
			       dsp, dsp - hostdata->pScript);
			NCR_700_scsi_done(hostdata, SCp, DID_ERROR<<16);
		} else if(dstat & (WATCH_DOG_INTERRUPT|ABORTED)) {
			printk(KERN_ERR "scsi%d: (%d:%d) serious DMA problem, dstat=%02x\n",
			       host->host_no, pun, lun, dstat);
			NCR_700_scsi_done(hostdata, SCp, DID_ERROR<<16);
		}

		
		/* NOTE: selection interrupt processing MUST occur
		 * after script interrupt processing to correctly cope
		 * with the case where we process a disconnect and
		 * then get reselected before we process the
		 * disconnection */
		if(sstat0 & SELECTED) {
			/* FIXME: It currently takes at least FOUR
			 * interrupts to complete a command that
			 * disconnects: one for the disconnect, one
			 * for the reselection, one to get the
			 * reselection data and one to complete the
			 * command.  If we guess the reselected
			 * command here and prepare it, we only need
			 * to get a reselection data interrupt if we
			 * guessed wrongly.  Since the interrupt
			 * overhead is much greater than the command
			 * setup, this would be an efficient
			 * optimisation particularly as we probably
			 * only have one outstanding command on a
			 * target most of the time */

			resume_offset = process_selection(host, dsp);

		}

	}

	if(resume_offset) {
		if(hostdata->state != NCR_700_HOST_BUSY) {
			printk(KERN_ERR "scsi%d: Driver error: resume at 0x%08x [0x%04x] with non busy host!\n",
			       host->host_no, resume_offset, resume_offset - hostdata->pScript);
			hostdata->state = NCR_700_HOST_BUSY;
		}

		DEBUG(("Attempting to resume at %x\n", resume_offset));
		NCR_700_clear_fifo(host);
		NCR_700_writel(resume_offset, host, DSP_REG);
	} 
	/* There is probably a technical no-no about this: If we're a
	 * shared interrupt and we got this interrupt because the
	 * other device needs servicing not us, we're still going to
	 * check our queued commands here---of course, there shouldn't
	 * be any outstanding.... */
	if(hostdata->state == NCR_700_HOST_FREE) {
		int i;

		for(i = 0; i < NCR_700_COMMAND_SLOTS_PER_HOST; i++) {
			/* fairness: always run the queue from the last
			 * position we left off */
			int j = (i + hostdata->saved_slot_position)
				% NCR_700_COMMAND_SLOTS_PER_HOST;
			
			if(hostdata->slots[j].state != NCR_700_SLOT_QUEUED)
				continue;
			if(NCR_700_start_command(hostdata->slots[j].cmnd)) {
				DEBUG(("scsi%d: Issuing saved command slot %p, cmd %p\t\n",
				       host->host_no, &hostdata->slots[j],
				       hostdata->slots[j].cmnd));
				hostdata->saved_slot_position = j + 1;
			}

			break;
		}
	}
 out_unlock:
	spin_unlock_irqrestore(&io_request_lock, flags);
}

/* FIXME: Need to put some proc information in and plumb it
 * into the scsi proc system */
STATIC int
NCR_700_proc_directory_info(char *proc_buf, char **startp,
			 off_t offset, int bytes_available,
			 int host_no, int write)
{
	static char buf[4096];	/* 1 page should be sufficient */
	int len = 0;
	struct Scsi_Host *host = scsi_hostlist;
	struct NCR_700_Host_Parameters *hostdata;
	Scsi_Device *SDp;

	while(host != NULL && host->host_no != host_no)
		host = host->next;

	if(host == NULL)
		return 0;

	if(write) {
		/* FIXME: Clear internal statistics here */
		return 0;
	}
	hostdata = (struct NCR_700_Host_Parameters *)host->hostdata[0];
	len += sprintf(&buf[len], "Total commands outstanding: %d\n", hostdata->command_slot_count);
	len += sprintf(&buf[len],"\
Target	Depth  Active  Next Tag\n\
======	=====  ======  ========\n");
	for(SDp = host->host_queue; SDp != NULL; SDp = SDp->next) {
		len += sprintf(&buf[len]," %2d:%2d   %4d    %4d      %4d\n", SDp->id, SDp->lun, SDp->queue_depth, NCR_700_get_depth(SDp), SDp->current_tag);
	}
	if((len -= offset) <= 0)
		return 0;
	if(len > bytes_available)
		len = bytes_available;
	memcpy(proc_buf, buf + offset, len);
	return len;
}

STATIC int
NCR_700_queuecommand(Scsi_Cmnd *SCp, void (*done)(Scsi_Cmnd *))
{
	struct NCR_700_Host_Parameters *hostdata = 
		(struct NCR_700_Host_Parameters *)SCp->host->hostdata[0];
	__u32 move_ins;
	int pci_direction;
	struct NCR_700_command_slot *slot;
	int hash;

	if(hostdata->command_slot_count >= NCR_700_COMMAND_SLOTS_PER_HOST) {
		/* We're over our allocation, this should never happen
		 * since we report the max allocation to the mid layer */
		printk(KERN_WARNING "scsi%d: Command depth has gone over queue depth\n", SCp->host->host_no);
		return 1;
	}
	if(NCR_700_get_depth(SCp->device) != 0 && !(hostdata->tag_negotiated & (1<<SCp->target))) {
		DEBUG((KERN_ERR "scsi%d (%d:%d) has non zero depth %d\n",
		       SCp->host->host_no, SCp->target, SCp->lun,
		       NCR_700_get_depth(SCp->device)));
		return 1;
	}
	if(NCR_700_get_depth(SCp->device) >= NCR_700_MAX_TAGS) {
		DEBUG((KERN_ERR "scsi%d (%d:%d) has max tag depth %d\n",
		       SCp->host->host_no, SCp->target, SCp->lun,
		       NCR_700_get_depth(SCp->device)));
		return 1;
	}
	NCR_700_set_depth(SCp->device, NCR_700_get_depth(SCp->device) + 1);

	/* begin the command here */
	/* no need to check for NULL, test for command_slot_cound above
	 * ensures a slot is free */
	slot = find_empty_slot(hostdata);

	slot->cmnd = SCp;

	SCp->scsi_done = done;
	SCp->host_scribble = (unsigned char *)slot;
	SCp->SCp.ptr = NULL;
	SCp->SCp.buffer = NULL;

#ifdef NCR_700_DEBUG
	printk("53c700: scsi%d, command ", SCp->host->host_no);
	print_command(SCp->cmnd);
#endif
	if(SCp->device->tagged_supported && !SCp->device->tagged_queue
	   && (hostdata->tag_negotiated &(1<<SCp->target)) == 0
	   && NCR_700_is_flag_clear(SCp->device, NCR_700_DEV_BEGIN_TAG_QUEUEING)) {
		/* upper layer has indicated tags are supported.  We don't
		 * necessarily believe it yet.
		 *
		 * NOTE: There is a danger here: the mid layer supports
		 * tag queuing per LUN.  We only support it per PUN because
		 * of potential reselection issues */
		printk(KERN_INFO "scsi%d: (%d:%d) Enabling Tag Command Queuing\n", SCp->device->host->host_no, SCp->target, SCp->lun);
		hostdata->tag_negotiated |= (1<<SCp->target);
		NCR_700_set_flag(SCp->device, NCR_700_DEV_BEGIN_TAG_QUEUEING);
		SCp->device->tagged_queue = 1;
	}

	if(hostdata->tag_negotiated &(1<<SCp->target)) {

		struct NCR_700_command_slot *old =
			find_ITL_Nexus(hostdata, SCp->target, SCp->lun);
#ifdef NCR_700_TAG_DEBUG
		struct NCR_700_command_slot *found;
#endif
		
		if(old != NULL && old->tag == SCp->device->current_tag) {
			/* On some badly starving drives, this can be
			 * a frequent occurance, so print the message
			 * only once */
			if(NCR_700_is_flag_clear(SCp->device, NCR_700_DEV_TAG_STARVATION_WARNED)) {
				printk(KERN_WARNING "scsi%d (%d:%d) Target is suffering from tag starvation.\n", SCp->host->host_no, SCp->target, SCp->lun);
				NCR_700_set_flag(SCp->device, NCR_700_DEV_TAG_STARVATION_WARNED);
			}
			/* Release the slot and ajust the depth before refusing
			 * the command */
			free_slot(slot, hostdata);
			NCR_700_set_depth(SCp->device, NCR_700_get_depth(SCp->device) - 1);
			return 1;
		}
		slot->tag = SCp->device->current_tag++;
#ifdef NCR_700_TAG_DEBUG
		while((found = find_ITLQ_Nexus(hostdata, SCp->target, SCp->lun, slot->tag)) != NULL) {
			printk("\n\n**ERROR** already using tag %d, but oldest is %d\n", slot->tag, (old == NULL) ? -1 : old->tag);
			printk("  FOUND = %p, tag = %d, pun = %d, lun = %d\n",
			       found, found->tag, found->cmnd->target, found->cmnd->lun);
			slot->tag = SCp->device->current_tag++;
			printk("   Tag list is: ");
			while(old != NULL) {
				if(old->cmnd->target == SCp->target &&
				   old->cmnd->lun == SCp->lun)
					printk("%d ", old->tag);
				old = old->ITL_back;
			}
			printk("\n\n");
		}
#endif
		hash = hash_ITLQ(SCp->target, SCp->lun, slot->tag);
		/* link into the ITLQ hash queues */
		slot->ITLQ_forw = hostdata->ITLQ_Hash_forw[hash];
		hostdata->ITLQ_Hash_forw[hash] = slot;
#ifdef NCR_700_TAG_DEBUG
		if(slot->ITLQ_forw != NULL && slot->ITLQ_forw->ITLQ_back != NULL) {
			printk(KERN_ERR "scsi%d (%d:%d) ITLQ_back is not NULL!!!!\n", SCp->host->host_no, SCp->target, SCp->lun);
		}
#endif
		if(slot->ITLQ_forw != NULL)
			slot->ITLQ_forw->ITLQ_back = slot;
		else
			hostdata->ITLQ_Hash_back[hash] = slot;
		slot->ITLQ_back = NULL;
	} else {
		slot->tag = NCR_700_NO_TAG;
	}
	/* link into the ITL hash queues */
	hash = hash_ITL(SCp->target, SCp->lun);
	slot->ITL_forw = hostdata->ITL_Hash_forw[hash];
	hostdata->ITL_Hash_forw[hash] = slot;
#ifdef NCR_700_TAG_DEBUG
	if(slot->ITL_forw != NULL && slot->ITL_forw->ITL_back != NULL) {
		printk(KERN_ERR "scsi%d (%d:%d) ITL_back is not NULL!!!!\n",
		       SCp->host->host_no, SCp->target, SCp->lun);
	}
#endif
	if(slot->ITL_forw != NULL)
		slot->ITL_forw->ITL_back = slot;
	else
		hostdata->ITL_Hash_back[hash] = slot;
	slot->ITL_back = NULL;

	/* sanity check: some of the commands generated by the mid-layer
	 * have an eccentric idea of their sc_data_direction */
	if(!SCp->use_sg && !SCp->request_bufflen 
	   && SCp->sc_data_direction != SCSI_DATA_NONE) {
#ifdef NCR_700_DEBUG
		printk("53c700: Command");
		print_command(SCp->cmnd);
		printk("Has wrong data direction %d\n", SCp->sc_data_direction);
#endif
		SCp->sc_data_direction = SCSI_DATA_NONE;
	}

	switch (SCp->cmnd[0]) {
	case REQUEST_SENSE:
		/* clear the internal sense magic */
		SCp->cmnd[6] = 0;
		/* fall through */
	default:
		/* OK, get it from the command */
		switch(SCp->sc_data_direction) {
		case SCSI_DATA_UNKNOWN:
		default:
			printk(KERN_ERR "53c700: Unknown command for data direction ");
			print_command(SCp->cmnd);
			
			move_ins = 0;
			break;
		case SCSI_DATA_NONE:
			move_ins = 0;
			break;
		case SCSI_DATA_READ:
			move_ins = SCRIPT_MOVE_DATA_IN;
			break;
		case SCSI_DATA_WRITE:
			move_ins = SCRIPT_MOVE_DATA_OUT;
			break;
		}
	}

	/* now build the scatter gather list */
	pci_direction = scsi_to_pci_dma_dir(SCp->sc_data_direction);
	if(move_ins != 0) {
		int i;
		int sg_count;
		dma_addr_t vPtr = 0;
		__u32 count = 0;

		if(SCp->use_sg) {
			sg_count = pci_map_sg(hostdata->pci_dev, SCp->buffer,
					      SCp->use_sg, pci_direction);
		} else {
			vPtr = pci_map_single(hostdata->pci_dev,
					      SCp->request_buffer, 
					      SCp->request_bufflen,
					      pci_direction);
			count = SCp->request_bufflen;
			slot->dma_handle = vPtr;
			sg_count = 1;
		}
			

		for(i = 0; i < sg_count; i++) {

			if(SCp->use_sg) {
				struct scatterlist *sg = SCp->buffer;

				vPtr = sg_dma_address(&sg[i]);
				count = sg_dma_len(&sg[i]);
			}

			slot->SG[i].ins = bS_to_host(move_ins | count);
			DEBUG((" scatter block %d: move %d[%08x] from 0x%lx\n",
			       i, count, slot->SG[i].ins, (unsigned long)vPtr));
			slot->SG[i].pAddr = bS_to_host(vPtr);
		}
		slot->SG[i].ins = bS_to_host(SCRIPT_RETURN);
		slot->SG[i].pAddr = 0;
		NCR_700_dma_cache_wback((unsigned long)slot->SG, sizeof(slot->SG));
		DEBUG((" SETTING %08lx to %x\n",
		       (&slot->pSG[i].ins), 
		       slot->SG[i].ins));
	}
	slot->resume_offset = 0;
	slot->pCmd = pci_map_single(hostdata->pci_dev, SCp->cmnd,
				    sizeof(SCp->cmnd), PCI_DMA_TODEVICE);
	NCR_700_start_command(SCp);
	return 0;
}

STATIC int
NCR_700_abort(Scsi_Cmnd * SCp)
{
	struct NCR_700_command_slot *slot;
	struct NCR_700_Host_Parameters *hostdata = 
		(struct NCR_700_Host_Parameters *)SCp->host->hostdata[0];

	printk(KERN_INFO "scsi%d (%d:%d) New error handler wants to abort command\n\t",
	       SCp->host->host_no, SCp->target, SCp->lun);
	print_command(SCp->cmnd);

	slot = find_ITL_Nexus(hostdata, SCp->target, SCp->lun);
	while(slot != NULL && slot->cmnd != SCp)
		slot = slot->ITL_back;

	if(slot == NULL)
		/* no outstanding command to abort */
		return SUCCESS;
	if(SCp->cmnd[0] == TEST_UNIT_READY) {
		/* FIXME: This is because of a problem in the new
		 * error handler.  When it is in error recovery, it
		 * will send a TUR to a device it thinks may still be
		 * showing a problem.  If the TUR isn't responded to,
		 * it will abort it and mark the device off line.
		 * Unfortunately, it does no other error recovery, so
		 * this would leave us with an outstanding command
		 * occupying a slot.  Rather than allow this to
		 * happen, we issue a bus reset to force all
		 * outstanding commands to terminate here. */
		NCR_700_internal_bus_reset(SCp->host);
		/* still drop through and return failed */
	}
	return FAILED;

}

STATIC int
NCR_700_bus_reset(Scsi_Cmnd * SCp)
{
	printk(KERN_INFO "scsi%d (%d:%d) New error handler wants BUS reset, cmd %p\n\t",
	       SCp->host->host_no, SCp->target, SCp->lun, SCp);
	print_command(SCp->cmnd);
	NCR_700_internal_bus_reset(SCp->host);
	return SUCCESS;
}

STATIC int
NCR_700_dev_reset(Scsi_Cmnd * SCp)
{
	printk(KERN_INFO "scsi%d (%d:%d) New error handler wants device reset\n\t",
	       SCp->host->host_no, SCp->target, SCp->lun);
	print_command(SCp->cmnd);
	
	return FAILED;
}

STATIC int
NCR_700_host_reset(Scsi_Cmnd * SCp)
{
	printk(KERN_INFO "scsi%d (%d:%d) New error handler wants HOST reset\n\t",
	       SCp->host->host_no, SCp->target, SCp->lun);
	print_command(SCp->cmnd);

	NCR_700_internal_bus_reset(SCp->host);
	NCR_700_chip_reset(SCp->host);
	return SUCCESS;
}

EXPORT_SYMBOL(NCR_700_detect);
EXPORT_SYMBOL(NCR_700_release);
EXPORT_SYMBOL(NCR_700_intr);
